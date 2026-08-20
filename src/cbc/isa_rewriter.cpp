#include "isa_rewriter.h"
#include "cbc/emitter/emitter.h"
#include "cbc/emitter/symbols.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/literals.h"
#include "interpreter/loggers.h"
#include "offsets_index.h"
#include "resolution/resolution.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include "utils/reinterpretation.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <variant>
#include <vector>

namespace Cbc {

using namespace Resolution;

using MemSpaceEmitter = Emitter::Emitter::MemSpace;

using TK  = CbcTypeKind;
using LDK = Format::LoadAccessKind;
using STK = Format::StoreAccessKind;

enum class New {
    Obj,
    Arr,
};

static LDK Ldk(CbcTypeKind tk)
{
    switch (tk) {
        case TK::U8:  return LDK::LD_U8;
        case TK::I8:  return LDK::LD_S8;
        case TK::U16: return LDK::LD_U16;
        case TK::I16: return LDK::LD_S16;
        case TK::U32:
        case TK::I32: return LDK::LD_32;
        case TK::U64:
        case TK::I64: return LDK::LD_64;
        case TK::F32: return LDK::LD_F32;
        case TK::F64: return LDK::LD_F64;

        case TK::BOOL: return LDK::LD_U8;
        case TK::REF:  return LDK::LD_REF;
        case TK::REC:  return LDK::LD_LEA; // record types: load effective address

        default: {
            FATAL("Not supported type kind %d", tk);
            return LDK::LD_S8;
        }
    }
}

static STK Stk(TK tk)
{
    switch (tk) {
        case TK::U8:
        case TK::I8:  return STK::ST_8;
        case TK::U16:
        case TK::I16: return STK::ST_16;
        case TK::U32:
        case TK::I32: return STK::ST_32;
        case TK::U64:
        case TK::I64: return STK::ST_64;
        case TK::F32: return STK::ST_F32;
        case TK::F64: return STK::ST_F64;

        case TK::BOOL: return STK::ST_8;
        case TK::REF:  return STK::ST_REF;

        default: {
            FATAL("Not supported type kind %d", tk);
            return STK::ST_8;
        }
    }
}

STK Stk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_UNIT:    ASSERT("Should not reach here");
        case Interpretation::BUILTIN_BOOLEAN: return STK::ST_8;
        case Interpretation::BUILTIN_U8:      return STK::ST_8;
        case Interpretation::BUILTIN_I8:      return STK::ST_8;
        case Interpretation::BUILTIN_U16:     return STK::ST_16;
        case Interpretation::BUILTIN_I16:     return STK::ST_16;
        case Interpretation::BUILTIN_U32:     return STK::ST_32;
        case Interpretation::BUILTIN_I32:     return STK::ST_32;
        case Interpretation::BUILTIN_U64:     return STK::ST_64;
        case Interpretation::BUILTIN_I64:     return STK::ST_64;
        case Interpretation::BUILTIN_UADDR:   return STK::ST_64;
        case Interpretation::BUILTIN_IADDR:   return STK::ST_64;
        case Interpretation::BUILTIN_F16:     return STK::ST_16;
        case Interpretation::BUILTIN_F32:     return STK::ST_F32;
        case Interpretation::BUILTIN_F64:     return STK::ST_F64;
        case Interpretation::BUILTIN_RUNE:    return STK::ST_32;
        case Interpretation::BUILTIN_CSTRING: return STK::ST_64;
    }
}

LDK Ldk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_UNIT:    ASSERT("Should not reach here");
        case Interpretation::BUILTIN_BOOLEAN: return LDK::LD_U8;
        case Interpretation::BUILTIN_U8:      return LDK::LD_U8;
        case Interpretation::BUILTIN_I8:      return LDK::LD_S8;
        case Interpretation::BUILTIN_U16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_I16:     return LDK::LD_S16;
        case Interpretation::BUILTIN_U32:     return LDK::LD_32;
        case Interpretation::BUILTIN_I32:     return LDK::LD_32;
        case Interpretation::BUILTIN_U64:     return LDK::LD_64;
        case Interpretation::BUILTIN_I64:     return LDK::LD_64;
        case Interpretation::BUILTIN_UADDR:   return LDK::LD_64;
        case Interpretation::BUILTIN_IADDR:   return LDK::LD_64;
        case Interpretation::BUILTIN_F16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_F32:     return LDK::LD_F32;
        case Interpretation::BUILTIN_F64:     return LDK::LD_F64;
        case Interpretation::BUILTIN_RUNE:    return LDK::LD_32;
        case Interpretation::BUILTIN_CSTRING: return LDK::LD_64;
    }
}

struct IsaRewriter : public IsaParser {
    IsaRewriter(
        Resolver& resolver,
        Engine::Session& session,
        Symlevel::Identifier<Symlevel::MethodDefinition> method,
        MethodCode& code,
        FrameLayout frameLayout,
        Emitter::Emitter& emit
    )
        : IsaParser(code),
          resolver(resolver),
          session(session),
          method(method),
          fileId(method.GetFileId()),
          code(code),
          emit(emit),
          frameLayout(frameLayout),
          startPosition(0),
          bytecodeSize(reader.End() - reader.Start())
    {}

    Engine::Session& session;
    Symlevel::Identifier<Symlevel::MethodDefinition> method;
    Symlevel::FileId fileId;
    Resolver& resolver;
    MethodCode& code;
    Emitter::Emitter& emit;
    FrameLayout frameLayout;
    size_t bytecodeSize;
    Stream::Output& errStream = Interpretation::Log::preparation.Stream(Logging::Level::ERROR);

    size_t startPosition;
    std::unordered_map<ssize_t, Emitter::Label> instructionLabel;

    // positions, where GC metadata is expected to be attached
    struct StatePoint {
        Emitter::Label label; // position in rewritten code
        ssize_t originalPos;  // position in original code
    };

    std::vector<StatePoint> statePoints;

    struct FailureMessage {
        size_t position;
        std::string message;
    };

    std::vector<FailureMessage> failureMessages;

    InstructionOffsetsIndex BuildOffsetsIndex() { return InstructionOffsetsIndex::Create(emit, instructionLabel); }

    Emitter::Label InstructionLabel(ssize_t position)
    {
        ASSERTION(position >= 0, "label position is negative");
        ASSERTION(position <= bytecodeSize, "label position greater than bytecode size");
        if (auto existing = instructionLabel.find(position); existing != instructionLabel.end()) {
            return existing->second;
        } else {
            auto label = emit.NewLabel();
            instructionLabel.insert({ position, label });
            return label;
        }
    }

    void BindStatePoint()
    {
        auto label = emit.NewLabel();
        emit.Bind(label);
        StatePoint point {
            .label       = label,
            .originalPos = Pos(), // attached to the end of instruction
        };
        statePoints.push_back(point);
    }

    template <typename Method> void EmitLogCall(std::string_view prefix, Method m)
    {
        if (Interpretation::Log::interpretation.GetLogLevel() < Logging::Level::TRACE) {
            return;
        }
        Stream::StringBuffer stream;
        stream << startPosition << ": " << prefix << ' ' << m;
        emit.LogInstruction(stream.ToCString());
    }

    char* returnedToMsg = nullptr;

    void EmitReturnedTo()
    {
        if (Interpretation::Log::interpretation.GetLogLevel() < Logging::Level::TRACE) {
            return;
        }
        if (!returnedToMsg) {
            Stream::StringBuffer buf;
            Stream::ResolvingOutput out(session, buf);
            out << "Returned to: " << method << ' ' << Stream::Detailed(method);
            returnedToMsg = buf.ToCString();
        }
        emit.LogInstruction(returnedToMsg);
    }

    ssize_t Pos()
    {
        auto start  = reader.Start();
        auto cursor = reader.Cursor();
        return cursor - start;
    }

    void AdjustReg(IReg expected, IReg actual)
    {
        if (expected != actual) {
            emit.Mov(expected, actual);
        }
    }

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        if (cc.IsFloatingPoint()) {
            // FIXME: support floats
            emit.Bcc(cc, width, FReg::From(l), FReg::From(r), InstructionLabel(Pos() + delta));
        } else {
            emit.Bcc(cc, width, IReg::From(l), IReg::From(r), InstructionLabel(Pos() + delta));
        }
    }

    void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta) override
    {
        emit.BccImm(cc, width, IReg::From(l), imm, InstructionLabel(Pos() + delta));
    }

    void Nop() override { emit.Nop(); }

    void Jump(int64_t delta) override { emit.Jmp(InstructionLabel(Pos() + delta)); }

    void Mov(Format::Width width, IReg d, IReg s) override { emit.Mov(d, s); }

    void FMov(Format::Width width, FReg d, FReg s) override { emit.Mov(d, s); }

    void FloatToInt(Format::Width width, IReg d, FReg s) override { emit.Mov(d, s); }

    void IntToFloat(Format::Width width, FReg d, IReg s) override { emit.Mov(d, s); }

    void MovRef(IReg d, IReg s) override { emit.Mov(d, s); }

    void MovImm(Format::Width width, IReg d, uint64_t value) override { emit.MovImm(width, d, value); }

    virtual void FMovImm(Format::Width width, FReg d, double value) override
    {
        if (width == Format::Width::W32) {
            emit.FMovI32(d, (float)value);
        } else {
            emit.FMovI64(d, value);
        }
    }

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        emit.Binary(op, width, d, l, r);
    }

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        emit.BinaryImm(op, width, d, l, value);
    }

    void CBinary(Format::Checked op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        emit.Binary(op, width, d, l, r);
    }

    void CBinaryImm(Format::Checked op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        emit.BinaryImm(op, width, d, l, value);
    }

    void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        emit.Binary(op, width, d, l, r);
    }

    void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s) override
    {
        emit.Unary(op, width, d, s);
    }

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        emit.Convert(toType, fromType, to, from);
    }

    void MovBasePtr(IReg dst, bool local) override
    {
        auto basePtr = local ? RTSupport::Execution::GetLocalBasePtr() : RTSupport::Execution::GetGlobalBasePtr();
        emit.MovImm(Format::Width::W64, dst, basePtr.value);
    }

    void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) override
    {
        // We can ignore resW and argW, because interpreter computes the result in 64-bit number anyway.
        if (sx) {
            emit.BFXS(dst, src, offset, size);
        } else {
            emit.BFXZ(dst, src, offset, size);
        }
    }

    void PrepareRecord(uint16_t ts) override
    {
        auto tsi = frameLayout.typedSlotsInfo[ts];
        auto ti  = RTSupport::TypeInfo(tsi.second);
        emit.PrepareTyped(ti, tsi.first);
    }

    void NewArr(IReg dst, IReg len, uint32_t typeId) override
    {
        AdjustReg(IReg::IR2, len);
        NewObject(dst, typeId, New::Arr);
    }

    void GcPoint() override
    {
        emit.GcPoint();
        BindStatePoint();
    }

    void LoadStackRec(IReg r, uint16_t ts) override
    {
        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, r, frameLayout.typedOffset.at(ts));
    }

    void LoadRawMemory(AnyReg dst, IReg base, int64_t offset, Format::LoadAccessKind ldk) override
    {
        emit.LoadRec(ldk, dst, base, offset);
    }

    void StoreRawMemory(AnyReg src, IReg base, int64_t offset, Format::StoreAccessKind stk) override
    {
        emit.StoreRec(stk, src, base, offset);
    }

    void LoadStatic(AnyReg r, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field  = f.value();
        auto symbol = emit.NewAddressSym(field->location);
        emit.LoadStatic(Ldk(field->fieldType.GetKind()), r, symbol);
    }

    void StoreStatic(AnyReg r, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field  = f.value();
        auto symbol = emit.NewAddressSym(field->location);
        emit.StoreStatic(Stk(field->fieldType.GetKind()), r, symbol);
    }

    void LoadField(IReg rb, AnyReg rd, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            emit.LoadObj(Ldk(field->fieldType.GetKind()), rd, rb, field->offset.value());
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreField(IReg rb, AnyReg rs, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            if (field->refType.GetKind() == Resolution::CbcTypeKind::REF) {
                emit.StoreObj(Stk(field->fieldType.GetKind()), rs, rb, field->offset.value());
            } else {
                emit.StoreRec(Stk(field->fieldType.GetKind()), rs, rb, field->offset.value());
            }
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void LoadTypeInfoGeneric(IReg dst, uint32_t typeId) override
    {
        using namespace Engine;
        auto type = resolver.Query(Index<Type>(typeId));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto term = resolver.termManager.Globalize(type->term);
        emit.LoadGenericTypeInfo(Bits::Raw64(term));
        AdjustReg(dst, IReg::IR1);
    }

    void LoadTypeInfoSig(IReg dst, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto ti = t->GetTypeInfo();
        if (!ti.has_value()) {
            errStream << "Failed to get type info of " << *t << Stream::endl;
            Fail();
            return;
        }
        emit.MovImm(Format::Width::W64, dst, ti->UInt());
    }

    void Offset(IReg dst, IReg ti, uint32_t fieldId, bool accumulate) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();

        // TODO: one instruction
        if (accumulate) {
            emit.Offset(IReg::IR_ACC, field->ordinal, ti);
            emit.Add(Format::Width::W64, dst, dst, IReg::IR_ACC);
        } else {
            emit.Offset(dst, field->ordinal, ti);
        }

        if (field->refType.GetKind() == Resolution::CbcTypeKind::REF) {
            emit.AddI(Format::Width::W64, dst, dst, RTSupport::MetaInfo::ObjectHeaderSize());
        }
    }

    void TagGeneric(IReg dst, IReg src, IReg ti, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Symlevel::Reader::Read(session, typeDefId);

        auto refPath = emit.NewLabel();
        auto end     = emit.NewLabel();
        emit.BranchIfRef(ti, refPath);
        emit.LoadObj(LDK::LD_U8, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        emit.Jmp(end);
        emit.Bind(refPath);

        emit.LoadObj(LDK::LD_REF, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        switch (typeDef->enumKind) {
            case Symlevel::EnumKind::OPTION0: // enum { Some(T), None }
                emit.SCC(Format::CC::REQ, Format::Width::W64, dst, dst, IReg::IRZ);
                break;
            case Symlevel::EnumKind::OPTION1: // enum { None, Some(T) }
                emit.SCC(Format::CC::RNE, Format::Width::W64, dst, dst, IReg::IRZ);
                break;
            default: return Fail("unexpected enum kind");
        }

        emit.Bind(end);
    }

    void PayloadGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Symlevel::Reader::Read(session, typeDefId);
        auto refPath   = emit.NewLabel();
        auto end       = emit.NewLabel();
        emit.BranchIfRef(underlyingTypeInfo, refPath);
        {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.GenericField(1, optionTypeInfo);
            ms.LoadGeneric(dst, src, underlyingTypeInfo);
            BindStatePoint();
        }
        emit.Jmp(end);
        emit.Bind(refPath);
        {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.LoadGeneric(dst, src, underlyingTypeInfo);
            BindStatePoint();
        }
        emit.Bind(end);
    }

    void NewNoneGeneric(IReg dst, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId) override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }
        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Symlevel::Reader::Read(session, typeDefId);

        auto end = emit.NewLabel();
        emit.NewObjGenericOnAcc(optionTypeInfo);
        BindStatePoint();
        emit.BranchIfRef(underlyingTypeInfo, end);
        if (typeDef->enumKind == Symlevel::EnumKind::OPTION1) {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.StoreObjImm(STK::ST_8, IReg::IR_ACC, 1);
        }
        emit.Bind(end);
        emit.Mov(dst, IReg::IR_ACC);
    }

    void NewSomeGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        auto t = resolver.Query(Index<Type>(optionTypeInfoId));
        if (!t.has_value()) {
            return Fail("resolution failure");
        }

        auto type      = *t;
        auto typeDefId = Engine::ExtractTypeDefIdentifier(type.term);
        auto typeDef   = Symlevel::Reader::Read(session, typeDefId);
        auto refPath   = emit.NewLabel();
        auto end       = emit.NewLabel();
        emit.NewObjGenericOnAcc(optionTypeInfo);
        BindStatePoint();
        emit.BranchIfRef(underlyingTypeInfo, refPath);
        {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.GenericField(1, optionTypeInfo);
            ms.StoreGeneric(src, IReg::IR_ACC, underlyingTypeInfo);
        }
        if (typeDef->enumKind == Symlevel::EnumKind::OPTION0) {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.StoreObjImm(STK::ST_8, IReg::IR_ACC, 1);
        }
        emit.Jmp(end);
        emit.Bind(refPath);
        {
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            ms.StoreGeneric(src, IReg::IR_ACC, underlyingTypeInfo);
        }
        emit.Bind(end);
        emit.Mov(dst, IReg::IR_ACC);
    }

    void AssignGeneric(IReg dst, IReg src, IReg ti) override { emit.AssignGeneric(dst, src, ti); }

    void InstanceOfGeneric(IReg dst, IReg obj, IReg ti) override { emit.InstanceOfGeneric(dst, obj, ti); }

    void AtomicLoad(IReg dst, IReg obj, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto ldk = Ldk(field->fieldType.GetKind());
        emit.AtomicLoad(dst, ldk, obj, field->offset.value());
    }

    void AtomicStore(IReg src, IReg obj, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        emit.AtomicStore(src, stk, obj, field->offset.value());
    }

    void CAS(IReg dst, IReg obj, IReg expected, IReg newVal, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:   opc = RT::Opcode::CAS_8; break;
            case Format::StoreAccessKind::ST_16:  opc = RT::Opcode::CAS_16; break;
            case Format::StoreAccessKind::ST_32:  opc = RT::Opcode::CAS_32; break;
            case Format::StoreAccessKind::ST_64:  opc = RT::Opcode::CAS_64; break;
            case Format::StoreAccessKind::ST_REF: opc = RT::Opcode::CAS_REF; break;
            default:                              FATAL("unexpected kind %d", stk);
        }
        emit.CAS(opc, dst, obj, expected, newVal, field->offset.value());
    }

    void AtomicSwap(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:   opc = RT::Opcode::ATOMIC_SWAP_8; break;
            case Format::StoreAccessKind::ST_16:  opc = RT::Opcode::ATOMIC_SWAP_16; break;
            case Format::StoreAccessKind::ST_32:  opc = RT::Opcode::ATOMIC_SWAP_32; break;
            case Format::StoreAccessKind::ST_64:  opc = RT::Opcode::ATOMIC_SWAP_64; break;
            case Format::StoreAccessKind::ST_REF: opc = RT::Opcode::ATOMIC_SWAP_REF; break;
            default:                              FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchAdd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_ADD_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_ADD_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_ADD_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_ADD_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchSub(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_SUB_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_SUB_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_SUB_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_SUB_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchAnd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_AND_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_AND_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_AND_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_AND_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchOr(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_OR_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_OR_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_OR_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_OR_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    void AtomicFetchXor(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (!field->offset.has_value()) {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return;
        }
        auto stk = Stk(field->fieldType.GetKind());
        RT::Opcode opc;
        switch (stk) {
            case Format::StoreAccessKind::ST_8:  opc = RT::Opcode::ATOMIC_FETCH_XOR_8; break;
            case Format::StoreAccessKind::ST_16: opc = RT::Opcode::ATOMIC_FETCH_XOR_16; break;
            case Format::StoreAccessKind::ST_32: opc = RT::Opcode::ATOMIC_FETCH_XOR_32; break;
            case Format::StoreAccessKind::ST_64: opc = RT::Opcode::ATOMIC_FETCH_XOR_64; break;
            default:                             FATAL("unexpected kind %d", stk);
        }
        emit.AtomicOp(opc, dst, obj, src, field->offset.value());
    }

    std::optional<Type> NewObject(IReg dst, uint32_t typeId, New kind)
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return std::nullopt;
        }
        auto ti = t->GetTypeInfo();
        if (!ti.has_value()) {
            errStream << "Failed to get type info of " << *t << Stream::endl;
            Fail();
            return std::nullopt;
        }

        auto typeInfo = *ti;
        switch (kind) {
            case New::Obj: emit.NewObj(typeInfo); break;
            case New::Arr: emit.NewArr(typeInfo); break;
        }
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
        return *t;
    }

    void NewObj(IReg dst, uint32_t typeId) override { NewObject(dst, typeId, New::Obj); }

    void CallDirect(IReg dst, uint32_t methodId) override
    {
        auto m = resolver.Query(Index<DirectCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();

        if (auto data = std::get_if<DirectCall::Compiled>(&method->data)) {
            EmitLogCall("call.2c", method);
            auto sym = emit.NewAddressSym(data->funcPtr);
            emit.DirectCall2c(sym);
            BindStatePoint();
        } else {
            EmitLogCall("call.2i", method);
            auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(method->data);
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(fuh));
            emit.DirectCall2i(sym);
            BindStatePoint();
        }
        AdjustReg(dst, IReg::IR1);
        EmitReturnedTo();
    }

    void CallVirtual(IReg dst, uint32_t methodId) override
    {
        auto m = resolver.Query(Index<VirtualCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        EmitLogCall("call.virt", method);
        emit.VirtualCall(method->methodNum, method->extDefNum, method->sret);
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
        EmitReturnedTo();
    }

    void CallInterf(IReg dst, uint32_t methodId) override
    {
        auto m = resolver.Query(Index<InterfaceCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        auto ti     = method->refType.GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        EmitLogCall("call.interf", method);
        emit.InterfaceCall(method->methodNum, *ti, method->sret);
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
        EmitReturnedTo();
    }

    void CallInterfGeneric(uint16_t argnum, uint32_t methodId) override
    {
        auto m = resolver.Query(Index<InterfaceCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        EmitLogCall("call.interf.g", method);
        emit.InterfaceCallGeneric(method->methodNum, argnum, method->sret);
        BindStatePoint();
        EmitReturnedTo();
    }

    void Spawn(IReg closure, uint32_t typeId) override
    {
        AdjustReg(IReg::IR1, closure);

        auto t = resolver.QueryFutureByFunctional(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type        = t.value();
        auto optTypeInfo = type.GetTypeInfo();
        if (!optTypeInfo.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = *optTypeInfo;
        emit.Spawn(typeInfo);
        BindStatePoint();
    }

    void SpawnFuture(IReg future, uint32_t type) override
    {
        BindStatePoint();
        FATAL("not implemented");
    }

    void CallClosure(IReg dst, uint32_t typeId, bool generic) override
    {
        if (generic) {
            // Generic calls of closure are always considered as `sret`.
            emit.CallClosureGeneric();
            BindStatePoint();
            return;
        }

        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value() || t->term.GetKind() != Engine::TermKind::FUNCTIONAL) {
            return Fail("failed to resolve type");
        }
        auto term    = t->term;
        auto retType = term.Subterm(term.GetLength() - 1);

        // For instantiated version of closure `sret` can be computed
        // by retType kind.
        bool sret = (resolver.Wrap(retType).GetKind() == TK::REC);
        emit.CallClosure(sret);
        BindStatePoint();
    }

    void NewClosure(IReg dst, uint32_t typeId) override
    {
        auto type = NewObject(IReg::IR1, typeId, New::Obj); // has BindStatePoint call inside
        if (!type.has_value()) {
            return;
        }

        auto typeDefId = Engine::TypeTermId(type->term).GetIdentifier();
        auto typeDef   = Symlevel::Reader::Read(resolver, typeDefId);

        // - Get 1th methodId out of iterator.
        // - Emit `InitClosure` with flags of 1th method.
        // - Ensure that there are only two methods in the closure type.
        int idx = 0;
        for (auto methodId : Symlevel::Reader::Resolve(resolver, typeDef.GetVirtualMethods())) {
            if (idx == 1) {
                auto method = Symlevel::Reader::Read(resolver, methodId);
                auto sret   = method.GetFlags().Is(Symlevel::MethodFlag::SRET);

                emit.InitClosure(sret);
                AdjustReg(dst, IReg::IR1);
            }
            idx++;
        }
        if (idx != 2) {
            Fail("failed to find instantiated version of method in closure");
        }
    }

    void NewObjGeneric(IReg ti, uint32_t typeId) override
    {
        emit.NewObjGeneric(ti);
        BindStatePoint();
    }

    void NewClosureGeneric(IReg ti, uint32_t typeId) override
    {
        auto type = resolver.Query(Index<Type>(typeId));
        if (!type.has_value()) {
            return;
        }

        emit.NewObjGeneric(ti);
        BindStatePoint();

        auto typeDefId = Engine::TypeTermId(type->term).GetIdentifier();
        auto typeDef   = Symlevel::Reader::Read(resolver, typeDefId);

        // - Get 1th methodId out of iterator.
        // - Emit `InitClosure` with flags of 1th method.
        // - Ensure that there are only two methods in the closure type.
        int idx = 0;
        for (auto methodId : Symlevel::Reader::Resolve(resolver, typeDef.GetVirtualMethods())) {
            if (idx == 1) {
                auto method = Symlevel::Reader::Read(resolver, methodId);
                auto sret   = method.GetFlags().Is(Symlevel::MethodFlag::SRET);

                emit.InitClosure(sret);
            }
            idx++;
        }
        if (idx != 2) {
            Fail("failed to find instantiated version of method in closure");
        }
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        if (cc.IsFloatingPoint()) {
            emit.SCC(cc, width, d, FReg::From(l), FReg::From(r));
        } else {
            emit.SCC(cc, width, d, IReg::From(l), IReg::From(r));
        }
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        emit.SCCImm(cc, width, d, l, imm);
    }

    void Ret(Format::Width width, IReg src) override
    {
        AdjustReg(IReg::IR1, src);
        emit.Ret();
    }

    void FRet(Format::Width width, FReg src) override
    {
        if (src != FReg::FR0) {
            emit.Mov(FReg::FR0, src);
        }
        emit.Ret();
    }

    void RetRef(IReg src) override
    {
        AdjustReg(IReg::IR1, src);
        emit.Ret();
    }

    void DivCheck(IReg reg) override { emit.DivCheck(reg); }

    void NullCheck(IReg reg) override { emit.NullCheck(reg); }

    void Catch(IReg reg) override { emit.Catch(reg); }

    void Throw(IReg reg) override { emit.Throw(reg); }

    void InstanceOf(IReg dst, IReg obj, uint32_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type = t.value();
        if (!type.GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << type << Stream::endl;
            Fail();
            return;
        }

        auto typeInfo = type.GetTypeInfo().value();
        emit.InstanceOf(dst, obj, typeInfo);
    }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { emit.LoadObj(Format::LoadAccessKind::LD_64, dst, obj, 0); }

    void InitObj(uint16_t ts) override { FATAL("not implemented"); }

    void InitString(uint16_t ts, uint32_t offset) override
    {
        auto str = resolver.QueryString(offset);
        // FIXME: string intern!
        if (str.size() > UINT32_MAX) {
            // TODO: log
            Fail();
        }
        // TODO: allocate proper array in heap?
        size_t size = str.size();
        auto mem    = std::malloc(sizeof(Interpretation::StringStorage) + size + 1);
        if (!mem) {
            FATAL("out of memory"); // FIXME: rewrite to throwing stub
        }

        auto storage = new (mem) Interpretation::StringStorage { RTSupport::MetaInfo::ByteArrayTypeInfo(), size };
        std::memcpy(storage->string, str.data(), size);
        storage->string[size] = 0;
        emit.StringLit(storage, frameLayout.typedOffset.at(ts));
    }

    void ArrayLength(IReg dst, IReg arr) override { FATAL("not implemented"); }

    void ArrayIndexCheck(IReg length, IReg index) override { FATAL("not implemented"); }

    static uint32_t UntypedSlotOffset(uint16_t us) { return us * STACK_SLOT_SIZE; }

    void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us) override
    {
        emit.LoadFrame(ldk, dst, UntypedSlotOffset(us));
    }

    void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) override
    {
        emit.StoreFrame(stk, src, UntypedSlotOffset(us));
    }

    void StoreUntypedImm(uint64_t imm, uint16_t us) override
    {
        emit.StoreFrameImm(Format::StoreAccessKind::ST_64, imm, UntypedSlotOffset(us));
    }

    void LoadTyped(AnyReg dst, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = frameLayout.typedOffset.at(ts) + field->offset.value();
            emit.LoadFrame(Ldk(field->fieldType.GetKind()), dst, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreTyped(AnyReg src, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = frameLayout.typedOffset.at(ts) + field->offset.value();
            emit.StoreFrame(Stk(field->fieldType.GetKind()), src, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void StoreTypedImm(uint64_t imm, uint16_t ts, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            auto offset = frameLayout.typedOffset.at(ts) + field->offset.value();
            emit.StoreFrameImm(Stk(field->fieldType.GetKind()), imm, offset);
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
        }
    }

    void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx) override
    {
        emit.LoadArray(ldk, dst, arr, idx);
    }

    void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) override
    {
        emit.StoreArray(stk, src, arr, idx);
    }

    void TypeArg(IReg ti, int idx, IReg dst) override { emit.TypeArg(dst, ti, idx); }

    Interpretation::BuiltinType ToBuiltin(Engine::TermKind tk)
    {
        using namespace Interpretation;
        switch (tk) {
            case Engine::TermKind::UNIT:    return BUILTIN_UNIT;
            case Engine::TermKind::BOOLEAN: return BUILTIN_BOOLEAN;
            case Engine::TermKind::U8:      return BUILTIN_U8;
            case Engine::TermKind::I8:      return BUILTIN_I8;
            case Engine::TermKind::U16:     return BUILTIN_U16;
            case Engine::TermKind::I16:     return BUILTIN_I16;
            case Engine::TermKind::U32:     return BUILTIN_U32;
            case Engine::TermKind::I32:     return BUILTIN_I32;
            case Engine::TermKind::U64:     return BUILTIN_U64;
            case Engine::TermKind::I64:     return BUILTIN_I64;
            case Engine::TermKind::UADDR:   return BUILTIN_UADDR;
            case Engine::TermKind::IADDR:   return BUILTIN_IADDR;
            case Engine::TermKind::F16:     return BUILTIN_F16;
            case Engine::TermKind::F32:     return BUILTIN_F32;
            case Engine::TermKind::F64:     return BUILTIN_F64;
            case Engine::TermKind::UCHAR32: return BUILTIN_RUNE;
            case Engine::TermKind::BSTRING: return BUILTIN_CSTRING;

            default: Fail("unexpected builtin kind"); return Interpretation::BUILTIN_I64;
        }
    }

    void Box(AnyReg src, IReg dst, uint32_t type) override
    {
        if (type != Interpretation::BUILTIN_UNIT && type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk = Engine::TermKind(type);
            auto bt = ToBuiltin(tk);
            emit.NewBox(bt); // Spoils IR_ACC
            BindStatePoint();
            emit.StoreObj(Stk(bt), src, IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize());
            AdjustReg(dst, IReg::IR_ACC);
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto type = t.value();
            auto ti   = type.GetTypeInfo();
            if (!ti.has_value()) {
                Fail();
                return;
            }
            auto typeInfo = ti.value();
            auto isrc     = IReg::From(src);

            emit.NewBox(typeInfo); // Spoils IR_ACC
            BindStatePoint();
            auto ms = emit.OpenMemSpace();
            ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
            if (type.GetKind() == CbcTypeKind::REF) {
                ms.StoreObj(Format::StoreAccessKind::ST_REF, isrc, IReg::IR_ACC);
            } else {
                ms.WriteStructFieldObj(isrc, IReg::IR_ACC, typeInfo);
            }
            AdjustReg(dst, IReg::IR_ACC);
        }
    }

    void BoxT(uint16_t srcTs, IReg dst) override
    {
        auto type = resolver.Query(Index<Type>(code.StackAllocSigs()[srcTs]));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto ti = type.value().GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = ti.value();
        auto offset   = frameLayout.typedOffset[srcTs];
        emit.NewBox(typeInfo);
        BindStatePoint();
        AdjustReg(dst, IReg::IR_ACC);
        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, IReg::IR_ACC, offset);
        auto ms = emit.OpenMemSpace();
        ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
        ms.WriteStructFieldObj(IReg::IR_ACC, dst, typeInfo);
    }

    void Unbox(AnyReg dst, IReg src, uint32_t type) override
    {
        if (type != Interpretation::BUILTIN_UNIT && type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk = Engine::TermKind(type);
            auto bt = ToBuiltin(tk);
            emit.LoadObj(Ldk(bt), dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto type = t.value();
            if (type.GetKind() == CbcTypeKind::REF) {
                emit.LoadObj(Format::LoadAccessKind::LD_REF, dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
            } else {
                auto ti = type.GetTypeInfo();
                if (!ti.has_value()) {
                    Fail();
                    return;
                }
                auto typeInfo = ti.value();
                emit.LoadObj(
                    Format::LoadAccessKind::LD_LEA, IReg::IR_ACC, src, RTSupport::MetaInfo::ObjectHeaderSize()
                );
                emit.ReadStructField(IReg::From(dst), src, IReg::IR_ACC, typeInfo);
            }
        }
    }

    void UnboxT(uint16_t dstTs, IReg src) override
    {
        auto type = resolver.Query(Index<Type>(code.StackAllocSigs()[dstTs]));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto ti = type.value().GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = ti.value();
        auto offset   = frameLayout.typedOffset[dstTs];
        emit.LoadFrame(Format::LoadAccessKind::LD_LEA, IReg::IR_ACC, offset);
        auto ms = emit.OpenMemSpace();
        ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
        ms.ReadStructFieldObj(IReg::IR_ACC, src, typeInfo);
    }

    enum HeadKind {
        HEAD_NONE,
        HEAD_OBJ,
        HEAD_REC,
        HEAD_DERIVED,
        HEAD_STATIC,
        HEAD_FRAME
    };

    struct MemSpaceRewriter : public MemSpace {
        MemSpaceRewriter(MemSpaceEmitter emit) : MemSpace(), emit(emit), lastFieldKind(CbcTypeKind::INVALID) {}

        HeadKind kind { HEAD_NONE };
        MemSpaceEmitter emit;
        IReg base { IReg::IRZ };
        IReg derived { IReg::IRZ };
        CbcTypeKind lastFieldKind;
    };

    bool FieldOffset(MemSpaceRewriter& msr, uint32_t fieldId)
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return false;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            // TODO: accumulate offset for field sequence
            msr.emit.Offset(field->offset.value());
            msr.lastFieldKind = field->fieldType.GetKind();
            return field->refType.GetKind() == CbcTypeKind::REF;
        } else {
            errStream << "Failed to get offset of field " << field << Stream::endl;
            Fail();
            return false;
        }
    }

    std::unique_ptr<MemSpace> OpenMemSpace() override
    {
        return std::make_unique<MemSpaceRewriter>(MemSpaceRewriter(emit.OpenMemSpace()));
    }

    void MemHeadReg(MemSpace& ms, IReg base, bool isRef) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.base  = base;
        msr.kind  = isRef ? HEAD_OBJ : HEAD_REC;
    }

    void MemHeadField(MemSpace& ms, IReg base, uint32_t fieldId) override
    {
        auto& msr  = static_cast<MemSpaceRewriter&>(ms);
        msr.base   = base;
        auto isRef = FieldOffset(msr, fieldId);
        msr.kind   = isRef ? HEAD_OBJ : HEAD_REC;
    }

    void MemHeadStatic(MemSpace& ms, uint32_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();

        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(field->location);
        msr.lastFieldKind = field->fieldType.GetKind();
        msr.kind          = HEAD_STATIC;
    }

    void MemHeadHandle(MemSpace& ms, IReg base, IReg derived) override
    {
        auto& msr   = static_cast<MemSpaceRewriter&>(ms);
        msr.base    = base;
        msr.derived = derived;
        msr.kind    = HEAD_DERIVED;
    }

    void MemHeadTyped(MemSpace& ms, uint16_t ts) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(frameLayout.typedOffset.at(ts));
        msr.kind = HEAD_FRAME;
    }

    void MemBodyField1(MemSpace& ms, uint32_t f1) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
    }

    void MemBodyField2(MemSpace& ms, uint32_t f1, uint32_t f2) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
    }

    void MemBodyField3(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
        FieldOffset(msr, f3);
    }

    void MemBodyField4(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
        FieldOffset(msr, f3);
        FieldOffset(msr, f4);
    }

    void MemBodyConstIndex(MemSpace& ms, int64_t idx, uint32_t refType) override
    {
        // FIXME: elem type is computable, remove `refType` from encoding.
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        auto t    = resolver.Query(Index<Type>(refType));
        if (!t.has_value()) {
            Fail();
            return;
        }
        // FIXME: support const indicies for arrays
        auto f = resolver.QueryTupleElement(t.value(), idx);
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = *f;
        if (!field->offset.has_value()) {
            Fail();
            return;
        }
        auto offset = *field->offset;
        msr.emit.Offset(offset);
        msr.lastFieldKind = field->fieldType.GetKind();
    }

    void MemBodyIndex(MemSpace& ms, IReg reg, uint32_t typeId, bool checked) override
    {
        if (checked) {
            FATAL("Not implemented checked MemBodyIndex");
            Fail();
            return;
        }

        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type = t.value();
        if (!type.GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << type << Stream::endl;
            Fail();
            return;
        }

        ASSERT(type.GetKind() == CbcTypeKind::REC);

        auto size = type.GetFlatSize();
        if (!size.has_value()) {
            Fail();
            return;
        }

        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(RTSupport::MetaInfo::ArrayBodyOffset());
        msr.emit.OffsetRegIdx(reg, *size);
    }

    void MemTailLoad(MemSpace& ms, IReg dst, std::vector<uint32_t> refs) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        for (auto r : refs) {
            FieldOffset(msr, r);
        }
        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.LoadObj(Ldk(msr.lastFieldKind), dst, msr.base); break;
            case HEAD_REC:     msr.emit.LoadRec(Ldk(msr.lastFieldKind), dst, msr.base); break;
            case HEAD_DERIVED: msr.emit.LoadDerived(Ldk(msr.lastFieldKind), dst, msr.base, msr.derived); break;
            case HEAD_FRAME:   msr.emit.LoadFrame(Ldk(msr.lastFieldKind), dst); break;
            case HEAD_STATIC:
                // IRZ means static record field, so whole position is encoded in accumulated offset
                // FIXME: encode as separate operation
                msr.emit.LoadRec(Ldk(msr.lastFieldKind), dst, IReg::IRZ);
                break;
            case HEAD_NONE: FATAL("unreachable");
        }
    }

    void MemTailStore(MemSpace& ms, IReg src, std::vector<uint32_t> refs) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        for (auto r : refs) {
            FieldOffset(msr, r);
        }
        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.StoreObj(Stk(msr.lastFieldKind), src, msr.base); break;
            case HEAD_REC:     msr.emit.StoreRec(Stk(msr.lastFieldKind), src, msr.base); break;
            case HEAD_DERIVED: msr.emit.StoreDerived(Stk(msr.lastFieldKind), src, msr.base, msr.derived); break;
            case HEAD_FRAME:   msr.emit.StoreFrame(Stk(msr.lastFieldKind), src); break;
            case HEAD_STATIC:
                // IRZ means static record field, so whole position is encoded in accumulated offset
                // FIXME: encode as separate operation
                msr.emit.StoreRec(Stk(msr.lastFieldKind), src, IReg::IRZ);
                break;
            case HEAD_NONE: FATAL("unreachable");
        }
    }

    void MemTailCopyRegTo(MemSpace& ms, IReg to, uint32_t recType) override
    {
        auto& msr    = static_cast<MemSpaceRewriter&>(ms);
        auto optType = resolver.Query(Index<Type>(recType));

        if (!optType.has_value()) {
            FATAL("Failed during copying of record: unknown record type.");
        }

        auto ty = *optType;

        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.CopyRecFromObj(msr.base, to, *ty.GetTypeInfo()); break;
            case HEAD_REC:     msr.emit.CopyRecFromRec(msr.base, to, *ty.GetTypeInfo()); break;
            case HEAD_DERIVED: msr.emit.CopyRecFromDerived(msr.base, msr.derived, to, *ty.GetTypeInfo()); break;
            case HEAD_STATIC:
                // IRZ means static record field, so whole position is encoded in accumulated offset
                // FIXME: encode as separate operation
                msr.emit.CopyRecFromObj(IReg::IRZ, to, *ty.GetTypeInfo());
                break;
                break;
            case HEAD_FRAME:
                ASSERTION(RTSupport::Execution::GetLocalBasePtr().value == 0, "assumes local base is zero");
                msr.emit.CopyRecFromRec(IReg::IRZ, to, *ty.GetTypeInfo());
                break;
            case HEAD_NONE: FATAL("unreachable");
        }
    }

    void MemTailCopyRegFrom(MemSpace& ms, IReg from, uint32_t recType) override
    {
        auto& msr    = static_cast<MemSpaceRewriter&>(ms);
        auto optType = resolver.Query(Index<Type>(recType));

        if (!optType.has_value()) {
            FATAL("Failed during copying of record: unknown record type.");
        }

        auto ty = *optType;

        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.CopyRecToObj(from, msr.base, *ty.GetTypeInfo()); break;
            case HEAD_REC:     msr.emit.CopyRecToRec(from, msr.base, *ty.GetTypeInfo()); break;
            case HEAD_DERIVED: msr.emit.CopyRecToDerived(msr.base, msr.derived, from, *ty.GetTypeInfo()); break;
            case HEAD_STATIC:
                // IRZ means static record field, so whole position is encoded in accumulated offset
                // FIXME: encode as separate operation
                msr.emit.CopyRecToObj(from, IReg::IRZ, *ty.GetTypeInfo());
                break;
                break;
            case HEAD_FRAME:
                ASSERTION(RTSupport::Execution::GetLocalBasePtr().value == 0, "assumes local base is zero");
                msr.emit.CopyRecToRec(from, IReg::IRZ, *ty.GetTypeInfo());
                break;
            case HEAD_NONE:    FATAL("unreachable");
        }
    }

    void MemTailStoreImm(MemSpace& ms, uint64_t imm) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.StoreObjImm(Stk(msr.lastFieldKind), msr.base, imm); break;
            case HEAD_REC:     msr.emit.StoreRecImm(Stk(msr.lastFieldKind), msr.base, imm); break;
            case HEAD_DERIVED: msr.emit.StoreDerivedImm(Stk(msr.lastFieldKind), msr.base, msr.derived, imm); break;
            case HEAD_FRAME:   msr.emit.StoreFrameImm(Stk(msr.lastFieldKind), imm); break;
            case HEAD_STATIC:
                // IRZ means static record field, so whole position is encoded in accumulated offset
                // FIXME: encode as separate operation
                msr.emit.StoreRecImm(Stk(msr.lastFieldKind), IReg::IRZ, imm);
                break;
            case HEAD_NONE: FATAL("unreachable");
        }
    }

    void MemBodyOffset(MemSpace& ms, IReg offset) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.OffsetReg(offset);
    }

    void MemBodyConstIndexGeneric(MemSpace& ms, int64_t idx, uint32_t elemType, IReg ti) override
    {
        FATAL("not implemented");
    }

    void MemBodyIndexGeneric(MemSpace& ms, IReg reg, uint32_t elemType, IReg ti) override { FATAL("not implemented"); }

    void MemBodyFieldGeneric(MemSpace& ms, uint32_t fieldId, IReg ti) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        auto f    = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->refType.GetKind() == Resolution::CbcTypeKind::REF) {
            msr.emit.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
        }
        msr.lastFieldKind = field->fieldType.GetKind();
        msr.emit.GenericField(field->ordinal, ti);
    }

    void MemTailStoreGeneric(MemSpace& ms, IReg src, IReg ti) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.StoreGeneric(src, msr.base, ti); break;
            case HEAD_DERIVED: msr.emit.StoreDerivedGeneric(src, msr.base, msr.derived, ti); break;
            case HEAD_REC:
            case HEAD_FRAME:
            case HEAD_STATIC:
            case HEAD_NONE:    FATAL("unexpected kind %d", msr.kind);
        }
    }

    void MemTailLoadGeneric(MemSpace& ms, IReg dst, IReg ti) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        switch (msr.kind) {
            case HEAD_OBJ:     msr.emit.LoadGeneric(dst, msr.base, ti); break;
            case HEAD_DERIVED: msr.emit.LoadDerivedGeneric(dst, msr.base, msr.derived, ti); break;
            case HEAD_REC:
            case HEAD_FRAME:
            case HEAD_STATIC:
            case HEAD_NONE:    FATAL("unexpected kind %d", msr.kind);
        }
        BindStatePoint();
    }

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        startPosition = position;
        emit.Bind(InstructionLabel(position));
        IsaParser::ParseOne();
    }

    void End() override
    {
        emit.Bind(InstructionLabel(Pos()));
        IsaParser::End();
    }

    void Fail(std::string&& msg = "") { failureMessages.push_back({ startPosition, std::move(msg) }); }

    void StopRewrite()
    {
        Fail();
        auto left = reader.End() - reader.Cursor();
        reader.Advance(left);
    }
};

static std::optional<FrameLayout> makeFrameLayout(Symlevel::Code code, Resolver& resolver)
{
    auto& log = Interpretation::Log::preparation;

    auto savedRegsCount = 0;
    for (uint8_t i = 0, savedRegs = code.UsedNonVolIRegMask(); i < (IReg::COUNT - IReg::FIRST_NON_VOL); i++) {
        if ((savedRegs & (1 << i)) != 0)
            savedRegsCount++;
    }
    for (uint8_t i = 0, savedRegs = code.UsedNonVolFRegMask(); i < (FReg::COUNT - FReg::FIRST_NON_VOL); i++) {
        if ((savedRegs & (1 << i)) != 0)
            savedRegsCount++;
    }
    auto savedRegsSpace = Cbc::STACK_SLOT_SIZE * savedRegsCount;

    auto untypedSlotsSize = Cbc::STACK_SLOT_SIZE * code.UntypedSlotCount();

    std::unordered_map<uint32_t, uint32_t> typedOffset;
    std::vector<std::pair<uint32_t, void*>> typedSlotsInfo;
    auto stackAllocSize = untypedSlotsSize;
    for (uint32_t i = 0; i < code.StackAllocSigsCount(); i++) {
        auto typeOpt = resolver.Query(Index<Type>(code.StackAllocSigs()[i]));
        if (!typeOpt.has_value()) {
            LOG_ERROR(log, "Failed to query type at stack-alloc index {}", i);
            return std::nullopt;
        }
        auto type = typeOpt.value();
        if (type.GetKind() != CbcTypeKind::REC) {
            LOG_ERROR(log, "Unexpected kind {} at t{} for type {}", (uint8_t)type.GetKind(), i, type);
            return std::nullopt;
        }
        auto size = type.GetFlatSize();
        if (!size.has_value()) {
            LOG_ERROR(log, "Unknown size at t{} for type {}", i, type);
            return std::nullopt;
        }
        auto typeInfo = type.GetTypeInfo();
        if (!typeInfo.has_value()) {
            LOG_ERROR(log, "Failed to obtain type info at t{} for type {}", i, type);
            return std::nullopt;
        }
        auto typeInfoPtr = typeInfo->Raw();

        typedOffset.insert({ i, stackAllocSize });
        typedSlotsInfo.push_back({ stackAllocSize, typeInfoPtr });
        stackAllocSize += MathUtils::AlignUp(size.value(), Cbc::STACK_SLOT_SIZE);
    }

    auto frameSize = MathUtils::AlignUp(savedRegsSpace + stackAllocSize, Cbc::FRAME_ALIGNMENT);

    return FrameLayout { std::move(typedOffset), std::move(typedSlotsInfo), untypedSlotsSize, frameSize };
}

static std::vector<Interpretation::GCPositionalInfo> CalculatePositionalGCInfo(
    Engine::Session& session,
    const MethodCode& code,
    Emitter::Emitter const& emitter,
    std::vector<IsaRewriter::StatePoint> const& statePoints
)
{
    auto livenessInfo = Symlevel::Reader::GetLivenessInfo(session, code);

    std::vector<Interpretation::GCPositionalInfo> posInfo;
    posInfo.reserve(livenessInfo.size());

    std::unordered_map<ssize_t, Symlevel::LivenessInfo const&> infos;
    for (const auto& info : livenessInfo) {
        infos.insert({ info.cbcPos, info });
    }

    for (auto& point : statePoints) {
        auto originalPos  = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it           = infos.find(originalPos);
        if (it == infos.end()) {
            FATAL("Unknown position");
        } else if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        }
        auto& info = it->second;

        posInfo.push_back({ .rewrittenPos        = (uint32_t)rewrittenPos,
                            .regMask             = info.regMask,
                            .untypedRefSlotsInfo = {},
                            .mutPairs            = {} });

        posInfo.back().untypedRefSlotsInfo.reserve(info.refSlotNums.size());
        for (const auto& slotN : info.refSlotNums) {
            posInfo.back().untypedRefSlotsInfo.push_back(slotN * STACK_SLOT_SIZE);
        }

        posInfo.back().mutPairs.reserve(info.mutPairs.size());
        for (const auto& pair : info.mutPairs) {
            auto mutRes = std::pair(
                Interpretation::Resource { .idx = pair.first }, Interpretation::Resource { .idx = pair.second }
            );
            posInfo.back().mutPairs.push_back(mutRes);
        }
    }

    return posInfo;
}

static std::vector<Interpretation::StackPtrsPositionalInfo> CalculateStackPtrsPositionalInfo(
    Engine::Session& session,
    const MethodCode& code,
    Emitter::Emitter const& emitter,
    std::vector<IsaRewriter::StatePoint> const& statePoints
)
{
    auto stackPtrsInfo = Symlevel::Reader::GetStackPtrsInfo(session, code);

    std::vector<Interpretation::StackPtrsPositionalInfo> posInfo;
    posInfo.reserve(stackPtrsInfo.size());

    // FIXME: the data must be stored in the format that is compact and fast to query.
    std::unordered_map<ssize_t, Symlevel::StackPtrsInfo const&> infos;
    for (const auto& info : stackPtrsInfo) {
        infos.insert({ info.cbcPos, info });
    }

    for (auto& point : statePoints) {
        auto originalPos  = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it           = infos.find(originalPos);
        if (it == infos.end()) {
            // Stack ptrs info is collected for a subset of state points
            continue;
        } else if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        }
        auto& info = it->second;

        Interpretation::StackPtrsPositionalInfo newInfo = { .rewrittenPos = (uint32_t)rewrittenPos, .resources = {} };

        newInfo.resources.reserve(info.resources.size());
        for (const auto& res : info.resources) {
            newInfo.resources.push_back(Interpretation::Resource { .idx = res });
        }
        posInfo.emplace_back(std::move(newInfo));
    }

    return posInfo;
}

static std::string Descriptor(Engine::Session& session, Symlevel::Identifier<Symlevel::MethodDefinition> method)
{
    Stream::StringBuffer buf;
    Stream::ResolvingOutput out(session, buf);
    out << method << " ";
    return buf.ToString();
}

Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session,
    MethodCode& code,
    Resolver& resolver,
    Memory::Heap& heap,
    Symlevel::Identifier<Symlevel::MethodDefinition> method
)
{
    using namespace Stream;
    Emitter::Emitter emitter;
    auto frameLayout = makeFrameLayout(code, resolver);

    if (!frameLayout.has_value()) {
        FATAL("Rewriter failed: cannot make frame layout.");
    }

    auto rewriter = IsaRewriter(resolver, session, method, code, *frameLayout, emitter);
    rewriter.ParseAll();

    if (!rewriter.failureMessages.empty()) {
        Interpretation::Log::preparation.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
            out << "Failed to rewrite method at positions: ";
            for (auto failure : rewriter.failureMessages) {
                out << failure.position << ": " << failure.message << endl;
            }
        });
        // FIXME: use stub that throws
        FATAL("Rewriter failed: cannot rewrite code.");
    }

    auto def     = Symlevel::Reader::Read(session, method);
    auto flags   = def.GetABIFlags();
    auto abiInfo = Interpretation::BuildAbiInfo(
        session,
        Engine::TermManager::Resolve(session, def.Signature()),
        {
            .isSRet            = flags.Is(Symlevel::MethodRefFlag::SRET),
            .isMut             = flags.Is(Symlevel::MethodRefFlag::MUT),
            .hasThisTypeInfo   = flags.Is(Symlevel::MethodRefFlag::HAS_THIS_TI),
            .hasOuterTi        = flags.Is(Symlevel::MethodRefFlag::HAS_OUTER_TI),
            .recordReceiver    = flags.Is(Symlevel::MethodRefFlag::REC_RECEIVER),
            .referenceReceiver = flags.Is(Symlevel::MethodRefFlag::REF_RECEIVER),
            .funcVars          = def->arity,
        }
    );

    auto rewrittenCode = emitter.Build(heap);

    return Interpretation::ExecBytecodeInfo {
        .code             = rewrittenCode,
        .savedIRegs       = Interpretation::NonVolatileRegs(code.UsedNonVolIRegMask() << IReg::FIRST_NON_VOL),
        .savedFRegs       = Interpretation::NonVolatileRegs(code.UsedNonVolFRegMask() << FReg::FIRST_NON_VOL),
        .frameSize        = frameLayout->frameSize,
        .untypedSlotCount = static_cast<uint16_t>(code.UntypedSlotCount()),
        .abiInfo          = std::move(abiInfo),
        .gcInfo =
            Interpretation::GcInfo {
                .positionalInfo = std::move(CalculatePositionalGCInfo(session, code, emitter, rewriter.statePoints)),
                .typedSlotsInfo = std::move((*frameLayout).typedSlotsInfo),
            },
        .stackPtrsInfo =
            Interpretation::StackPtrsInfo {
                .positionalInfo = CalculateStackPtrsPositionalInfo(session, code, emitter, rewriter.statePoints) },
        .offsetsIndex = rewriter.BuildOffsetsIndex(),
    };
}

Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, Symlevel::Identifier<Symlevel::MethodDefinition> method, Memory::Heap& heap
)
{
    using namespace Stream;
    auto def = Symlevel::Reader::Read(session, method);
    ASSERTION(def.MethodCode().has_value(), "fuh preparation must be unreachable for methods without code");

    Resolver resolver(session, method);
    auto code = Symlevel::Reader::Read(session, def.MethodCode().value());

    Interpretation::Log::preparation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));
        ResolvingOutput resolving(session, desc);
        resolving << code;
        Disasm(desc, code, &resolver);
    });

    auto res = Rewrite(session, code, resolver, heap, method);

    Interpretation::Log::preparation.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));

        desc.Print("bytecode: {} {}", Hex(res.code.bytecode), res.code.bytecodeSize);
        desc.NewLine();
        desc.Print("literals: {} {}", Hex(res.code.literals->_table), res.code.literals->_byteSize / 8);
        desc.NewLine();

        desc << res;
        Cbc::RT::Log(res.code, desc);
    });

    return res;
}

} // namespace Cbc
