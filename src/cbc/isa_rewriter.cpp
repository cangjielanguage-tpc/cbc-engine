#include "isa_rewriter.h"
#include "cbc/emitter/emitter.h"
#include "cbc/emitter/symbols.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "engine/resolving_output.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <variant>

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
        case TK::REC:  return LDK::LEA; // record types: load effective address

        default: {
            FATAL("Not supported template kind");
            return LDK::LD_S8;
        }
    }
}

static STK Stk(TK typeIdentifier)
{
    switch (typeIdentifier) {
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
            FATAL("Not supported template kind");
            return STK::ST_8;
        }
    }
}

STK Stk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_BOOLEAN: return STK::ST_8;
        case Interpretation::BUILTIN_U8:      return STK::ST_8;
        case Interpretation::BUILTIN_I8:      return STK::ST_8;
        case Interpretation::BUILTIN_U16:     return STK::ST_16;
        case Interpretation::BUILTIN_I16:     return STK::ST_16;
        case Interpretation::BUILTIN_U32:     return STK::ST_32;
        case Interpretation::BUILTIN_I32:     return STK::ST_32;
        case Interpretation::BUILTIN_U64:     return STK::ST_64;
        case Interpretation::BUILTIN_I64:     return STK::ST_64;
        case Interpretation::BUILTIN_F16:     return STK::ST_16;
        case Interpretation::BUILTIN_F32:     return STK::ST_F32;
        case Interpretation::BUILTIN_F64:     return STK::ST_F64;
    }
}

LDK Ldk(Interpretation::BuiltinType bt)
{
    switch (bt) {
        case Interpretation::BUILTIN_BOOLEAN: return LDK::LD_U8;
        case Interpretation::BUILTIN_U8:      return LDK::LD_U8;
        case Interpretation::BUILTIN_I8:      return LDK::LD_S8;
        case Interpretation::BUILTIN_U16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_I16:     return LDK::LD_S16;
        case Interpretation::BUILTIN_U32:     return LDK::LD_32;
        case Interpretation::BUILTIN_I32:     return LDK::LD_32;
        case Interpretation::BUILTIN_U64:     return LDK::LD_64;
        case Interpretation::BUILTIN_I64:     return LDK::LD_64;
        case Interpretation::BUILTIN_F16:     return LDK::LD_U16;
        case Interpretation::BUILTIN_F32:     return LDK::LD_F32;
        case Interpretation::BUILTIN_F64:     return LDK::LD_F64;
    }
}

struct IsaRewriter : public IsaParser {
    IsaRewriter(
        Resolver& resolver,
        Engine::Session& session,
        IO::FileId fileId,
        MethodCode& code,
        FrameLayout frameLayout,
        Emitter::Emitter& emit
    )
        : IsaParser(code),
          resolver(resolver),
          session(session),
          fileId(fileId),
          code(code),
          emit(emit),
          frameLayout(frameLayout),
          startPosition(0),
          bytecodeSize(reader.End() - reader.Start())
    {}

    Engine::Session& session;
    IO::FileId fileId;
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
        ssize_t originalPos; // position in original code
    };

    std::vector<StatePoint> statePoints;

    std::vector<size_t> failedPositions;

    // TODO: remove or it is needed for exceptions?
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

    void BindStatePoint() {
        auto label = emit.NewLabel();
        emit.Bind(label);
        StatePoint point {
            .label = label,
            .originalPos = Pos(), // attached to the end of instruction
        };
        statePoints.push_back(point);
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
        auto ti = RTSupport::TypeInfo(tsi.second);
        emit.PrepareTyped(ti, tsi.first);
    }

    void NewArr(IReg dst, IReg len, uint16_t typeId) override
    {
        AdjustReg(IReg::IR2, len);
        NewObject(dst, typeId, New::Arr);
    }

    void GcPoint() override
    {
        emit.GcPoint();
        BindStatePoint();
    }

    virtual void LoadStackRec(IReg r, uint16_t ts) override
    {
        emit.LoadFrame(Format::LoadAccessKind::LEA, r, frameLayout.typedOffset.at(ts));
    }

    void LoadStatic(AnyReg r, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field  = f.value();
        auto symbol = emit.NewAddressSym(field->location);
        emit.LoadStatic(Ldk(field->fieldType->GetKind()), r, symbol);
    }

    void StoreStatic(AnyReg r, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field  = f.value();
        auto symbol = emit.NewAddressSym(field->location);
        emit.StoreStatic(Stk(field->fieldType->GetKind()), r, symbol);
    }

    void LoadField(IReg rb, AnyReg rd, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            emit.LoadObj(Ldk(field->fieldType->GetKind()), rd, rb, field->offset.value());
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
            Fail();
        }
    }

    void StoreField(IReg rb, AnyReg rs, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<InstanceField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field = f.value();
        if (field->offset.has_value()) {
            if (field->refType->GetKind() == Resolution::CbcTypeKind::REF) {
                emit.StoreObj(Stk(field->fieldType->GetKind()), rs, rb, field->offset.value());
            } else {
                emit.StoreRec(Stk(field->fieldType->GetKind()), rs, rb, field->offset.value());
            }
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
            Fail();
        }
    }

    void LoadTypeInfoGeneric(IReg dst, uint16_t typeId) override
    {
        using namespace Engine;
        auto refId = Symlevel::RefId<Term>(0, typeId);
        auto ident = RefIdentifier<Term>(refId, fileId);
        auto term  = TermManager::Resolve(session, ident);
        if (term.GetKind() == TermKind::UNDEFINED) {
            Fail();
            return;
        }
        emit.LoadGenericTypeInfo(term.data);
        AdjustReg(dst, IReg::IR1);
    }

    void LoadTypeInfoSig(IReg dst, uint16_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type = t.value();
        if (!type->GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << *type << Stream::endl;
            Fail();
            return;
        }

        auto ti = type->GetTypeInfo().value();
        emit.MovImm(Format::Width::W64, dst, reinterpret_cast<uintptr_t>(ti.Raw()));
    }

    std::optional<Type*> NewObject(IReg dst, uint16_t typeId, New kind)
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return std::nullopt;
        }
        auto type = t.value();
        if (!type->GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << *type << Stream::endl;
            Fail();
            return std::nullopt;
        }

        auto typeInfo = type->GetTypeInfo().value();
        switch (kind) {
            case New::Obj: emit.NewObj(typeInfo); break;
            case New::Arr: emit.NewArr(typeInfo); break;
        }
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
        return type;
    }

    void NewObj(IReg dst, uint16_t typeId) override { NewObject(dst, typeId, New::Obj); }

    void CallDirect(IReg dst, uint16_t methodId) override
    {
        auto m = resolver.Query(Index<DirectCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();

        if (auto data = std::get_if<DirectCall::Compiled>(&method->data)) {
            auto sym = emit.NewAddressSym(data->funcPtr);
            emit.DirectCall2c(sym);
            BindStatePoint();
        } else {
            auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(method->data);
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(fuh));
            emit.DirectCall2i(sym);
            BindStatePoint();
        }
        AdjustReg(dst, IReg::IR1);
    }

    void CallVirtual(IReg dst, uint16_t methodId) override
    {
        auto m = resolver.Query(Index<VirtualCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        emit.VirtualCall(method->methodNum, method->extDefNum);
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
    }

    void CallInterf(IReg dst, uint16_t methodId) override
    {
        auto m = resolver.Query(Index<InterfaceCall>(methodId));
        if (!m.has_value()) {
            Fail();
            return;
        }
        auto method = m.value();
        auto ti     = method->refType->GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        emit.InterfaceCall(method->methodNum, *ti);
        BindStatePoint();
        AdjustReg(dst, IReg::IR1);
    }

    void Spawn(IReg closure, uint16_t typeId) override
    {
        AdjustReg(IReg::IR1, closure);

        auto t = resolver.QueryFutureByFunctional(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type        = t.value();
        auto optTypeInfo = type->GetTypeInfo();
        if (!optTypeInfo.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = *optTypeInfo;
        emit.Spawn(typeInfo);
        BindStatePoint();
    }

    void SpawnFuture(IReg future, uint16_t type) override
    {
        BindStatePoint();
        FATAL("not implemented");
    }

    void CallClosure(IReg dst, uint16_t type) override
    {
        BindStatePoint();
        FATAL("not implemented");
    }

    void NewClosure(IReg dst, uint16_t typeId) override
    {
        NewObj(IReg::IR1, typeId); // has BindStatePoint call inside
        emit.InitClosure();
        AdjustReg(dst, IReg::IR1);
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        if (cc.IsFloatingPoint()) {
            // FIXME: support for floats
            FATAL("not implemented");
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
        if (src != FReg::FR1) {
            emit.Mov(FReg::FR1, src);
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

    void Catch(IReg reg) override { FATAL("not implemented"); }

    void Throw(IReg reg) override { emit.Throw(reg); }

    void ZeroRefs(uint16_t ts) override { FATAL("not implemented"); }

    void InstanceOf(IReg dst, IReg obj, uint16_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto type = t.value();
        if (!type->GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << *type << Stream::endl;
            Fail();
            return;
        }

        auto typeInfo = type->GetTypeInfo().value();
        emit.InstanceOf(dst, obj, typeInfo);
    }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { FATAL("not implemented"); }

    void InitObj(uint16_t ts) override { FATAL("not implemented"); }

    void InitString(uint16_t ts, uint32_t offset) override
    {
        auto str = resolver.QueryString(offset);
        // FIXME: string intern!
        Interpretation::StringStorage* storage = nullptr;
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

        storage           = reinterpret_cast<Interpretation::StringStorage*>(mem);
        storage->size     = size;
        storage->typeInfo = RTSupport::MetaInfo::ByteArrayTypeInfo();

        std::memcpy(storage->string, str.data(), size);
        storage->string[size] = 0;
        emit.StringLit(storage, frameLayout.typedOffset.at(ts));
    }

    void ArrayLength(IReg dst, IReg arr) override { FATAL("not implemented"); }

    void ArrayIndexCheck(IReg length, IReg index) override { FATAL("not implemented"); }

    uint32_t UntypedSlotOffset(uint16_t us) { return us * STACK_SLOT_SIZE; }

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
            emit.LoadFrame(Ldk(field->fieldType->GetKind()), dst, offset);
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
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
            emit.StoreFrame(Stk(field->fieldType->GetKind()), src, offset);
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
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
            emit.StoreFrameImm(Stk(field->fieldType->GetKind()), imm, offset);
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
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

    void TypeArg(IReg ti, int idx, IReg dst) override { FATAL("Not implemented"); }

    Interpretation::BuiltinType ToBuiltin(Engine::TermKind tk)
    {
        switch (tk) {
            case Engine::TermKind::BOOLEAN: return Interpretation::BUILTIN_BOOLEAN;
            case Engine::TermKind::U8:      return Interpretation::BUILTIN_U8;
            case Engine::TermKind::I8:      return Interpretation::BUILTIN_I8;
            case Engine::TermKind::U16:     return Interpretation::BUILTIN_U16;
            case Engine::TermKind::I16:     return Interpretation::BUILTIN_I16;
            case Engine::TermKind::U32:     return Interpretation::BUILTIN_U32;
            case Engine::TermKind::I32:     return Interpretation::BUILTIN_I32;
            case Engine::TermKind::U64:     return Interpretation::BUILTIN_U64;
            case Engine::TermKind::I64:     return Interpretation::BUILTIN_I64;
            case Engine::TermKind::F16:     return Interpretation::BUILTIN_F16;
            case Engine::TermKind::F32:     return Interpretation::BUILTIN_F32;
            case Engine::TermKind::F64:     return Interpretation::BUILTIN_F64;

            default: Fail(); return Interpretation::BUILTIN_I64;
        }
    }

    void Box(AnyReg src, IReg dst, uint16_t type) override
    {
        if (type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk       = Engine::TermKind(type);
            auto term     = Engine::Term::Predefined(tk);
            auto& manager = Engine::TypeInfoManager::Of(session);
            auto bt       = ToBuiltin(tk);
            emit.NewBox(bt); // Spoils IR_ACC
            BindStatePoint();
            AdjustReg(dst, IReg::IR_ACC);
            emit.StoreObj(Stk(bt), src, dst, RTSupport::MetaInfo::ObjectHeaderSize());
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto ti = t.value()->GetTypeInfo();
            if (!ti.has_value()) {
                Fail();
                return;
            }
            auto typeInfo = ti.value();
            emit.NewBox(typeInfo); // Spoils IR_ACC
            BindStatePoint();
            AdjustReg(dst, IReg::IR_ACC);
            emit.LoadObj(
                Format::LoadAccessKind::LEA, IReg::IR_ACC, IReg::IR_ACC, RTSupport::MetaInfo::ObjectHeaderSize()
            );
            emit.WriteStructField(IReg::From(src), dst, IReg::IR_ACC, typeInfo);
        }
    }

    void BoxT(uint16_t srcTs, IReg dst) override
    {
        auto type = resolver.Query(Index<Type>(code.StackAllocSigs()[srcTs]));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto ti = type.value()->GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = ti.value();
        auto offset   = frameLayout.typedOffset[srcTs];
        emit.NewBox(typeInfo);
        BindStatePoint();
        AdjustReg(dst, IReg::IR_ACC);
        emit.LoadFrame(Format::LoadAccessKind::LEA, IReg::IR_ACC, offset);
        auto ms = emit.OpenMemSpace();
        ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
        ms.WriteStructFieldObj(IReg::IR_ACC, dst, typeInfo);
    }

    void Unbox(AnyReg dst, IReg src, uint16_t type) override
    {
        if (type < Engine::Term::FIRST_NON_PRIMITIVE) {
            auto tk       = Engine::TermKind(type);
            auto term     = Engine::Term::Predefined(tk);
            auto& manager = Engine::TypeInfoManager::Of(session);
            auto bt       = ToBuiltin(tk);
            emit.LoadObj(Ldk(bt), dst, src, RTSupport::MetaInfo::ObjectHeaderSize());
        } else {
            auto t = resolver.Query(Index<Type>(type));
            if (!t.has_value()) {
                Fail();
                return;
            }
            auto ti = t.value()->GetTypeInfo();
            if (!ti.has_value()) {
                Fail();
                return;
            }
            auto typeInfo = ti.value();
            emit.LoadObj(Format::LoadAccessKind::LEA, IReg::IR_ACC, src, RTSupport::MetaInfo::ObjectHeaderSize());
            emit.ReadStructField(IReg::From(dst), src, IReg::IR_ACC, typeInfo);
        }
    }

    void UnboxT(uint16_t dstTs, IReg src) override
    {
        auto type = resolver.Query(Index<Type>(code.StackAllocSigs()[dstTs]));
        if (!type.has_value()) {
            Fail();
            return;
        }
        auto ti = type.value()->GetTypeInfo();
        if (!ti.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = ti.value();
        auto offset   = frameLayout.typedOffset[dstTs];
        emit.LoadFrame(Format::LoadAccessKind::LEA, IReg::IR_ACC, offset);
        auto ms = emit.OpenMemSpace();
        ms.Offset(RTSupport::MetaInfo::ObjectHeaderSize());
        ms.ReadStructFieldObj(IReg::IR_ACC, src, typeInfo);
    }

    struct MemSpaceRewriter : public MemSpace {
        MemSpaceRewriter(MemSpaceEmitter emit)
            : MemSpace(),
            emit(emit),
            base(std::nullopt),
            derived(std::nullopt),
            ref(false),
            frame(false),
            lastFieldKind(CbcTypeKind::INVALID)
        {}

        MemSpaceEmitter emit;
        std::optional<IReg> base;
        std::optional<IReg> derived;
        bool ref;
        bool frame;
        CbcTypeKind lastFieldKind;
    };

    bool FieldOffset(MemSpaceRewriter& msr, uint16_t fieldId)
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
            msr.lastFieldKind = field->fieldType->GetKind();
            return field->refType->GetKind() == CbcTypeKind::REF;
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
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
        msr.base = base;
        msr.ref = isRef;
    }

    void MemHeadField(MemSpace& ms, IReg base, uint16_t fieldId) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.base = base;
        msr.ref = FieldOffset(msr, fieldId);
    }

    void MemHeadStatic(MemSpace& ms, uint16_t fieldId) override
    {
        auto f = resolver.Query(Index<StaticField>(fieldId));
        if (!f.has_value()) {
            Fail();
            return;
        }
        auto field  = f.value();

        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(field->location);
        msr.lastFieldKind = field->fieldType->GetKind();
    }

    void MemHeadHandle(MemSpace& ms, IReg base, IReg derived) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.base = base;
        msr.derived = derived;
    }

    void MemHeadTyped(MemSpace& ms, uint16_t ts) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(frameLayout.typedOffset.at(ts));
        msr.frame = true;
    }

    void MemBodyField1(MemSpace& ms, uint16_t f1) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
    }

    void MemBodyField2(MemSpace& ms, uint16_t f1, uint16_t f2) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
    }

    void MemBodyField3(MemSpace& ms, uint16_t f1, uint16_t f2, uint16_t f3) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
        FieldOffset(msr, f3);
    }

    void MemBodyField4(MemSpace& ms, uint16_t f1, uint16_t f2, uint16_t f3, uint16_t f4) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        FieldOffset(msr, f1);
        FieldOffset(msr, f2);
        FieldOffset(msr, f3);
        FieldOffset(msr, f4);
    }

    void MemBodyConstIndex(MemSpace& ms, int64_t idx, uint16_t refType) override
    {
        // FIXME: elem type is computable, remove `refType` from encoding.
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        auto t    = resolver.Query(Index<Type>(refType));
        if (!t.has_value()) {
            Fail();
            return;
        }
        // FIXME: support const indicies for arrays
        auto f = resolver.QueryTupleElement(*t, idx);
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
        msr.lastFieldKind = field->fieldType->GetKind();
    }

    void MemBodyIndex(MemSpace& ms, IReg reg, uint16_t typeId, bool checked) override
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
        if (!type->GetTypeInfo().has_value()) {
            errStream << "Failed to get type info of " << *type << Stream::endl;
            Fail();
            return;
        }

        ASSERT(type->GetKind() == CbcTypeKind::REC);

        auto size = type->GetFlatSize();
        if (!size.has_value()) {
            Fail();
            return;
        }

        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        msr.emit.Offset(RTSupport::MetaInfo::ArrayBodyOffset());
        msr.emit.OffsetRegIdx(reg, *size);
    }

    void MemTailLoad(MemSpace& ms, IReg dst, std::vector<uint16_t> refs) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        for (auto r : refs) {
            FieldOffset(msr, r);
        }
        if (msr.base.has_value()) {
            if (msr.derived.has_value()) {
                msr.emit.LoadDerived(Ldk(msr.lastFieldKind), dst, msr.base.value(), msr.derived.value());
            } else if (msr.ref) {
                msr.emit.LoadObj(Ldk(msr.lastFieldKind), dst, msr.base.value());
            } else {
                msr.emit.LoadRec(Ldk(msr.lastFieldKind), dst, msr.base.value());
            }
        } else if (msr.frame) {
            msr.emit.LoadFrame(Ldk(msr.lastFieldKind), dst);
        } else {
            // IRZ means static record field, so whole position is encoded in accumulated offset
            // FIXME: encode as separate operation
            msr.emit.LoadRec(Ldk(msr.lastFieldKind), dst, IReg::IRZ);
        }
    }

    void MemTailStore(MemSpace& ms, IReg src, std::vector<uint16_t> refs) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        for (auto r : refs) {
            FieldOffset(msr, r);
        }
        if (msr.base.has_value()) {
            if (msr.derived.has_value()) {
                msr.emit.StoreDerived(Stk(msr.lastFieldKind), src, msr.base.value(), msr.derived.value());
            } else if (msr.ref) {
                msr.emit.StoreObj(Stk(msr.lastFieldKind), src, msr.base.value());
            } else {
                msr.emit.StoreRec(Stk(msr.lastFieldKind), src, msr.base.value());
            }
        } else if (msr.frame) {
            msr.emit.StoreFrame(Stk(msr.lastFieldKind), src);
        } else {
            // IRZ means static record field, so whole position is encoded in accumulated offset
            // FIXME: encode as separate operation
            msr.emit.StoreRec(Stk(msr.lastFieldKind), src, IReg::IRZ);
        }
    }

    void MemTailStoreImm(MemSpace& ms, uint64_t imm) override
    {
        auto& msr = static_cast<MemSpaceRewriter&>(ms);
        if (msr.base.has_value()) {
            if (msr.derived.has_value()) {
                msr.emit.StoreDerivedImm(Stk(msr.lastFieldKind), msr.base.value(), msr.derived.value(), imm);
            } else if (msr.ref) {
                msr.emit.StoreObjImm(Stk(msr.lastFieldKind), msr.base.value(), imm);
            } else {
                msr.emit.StoreRecImm(Stk(msr.lastFieldKind), msr.base.value(), imm);
            }
        } else if (msr.frame) {
            msr.emit.StoreFrameImm(Stk(msr.lastFieldKind), imm);
        } else {
            // IRZ means static record field, so whole position is encoded in accumulated offset
            // FIXME: encode as separate operation
            msr.emit.StoreRecImm(Stk(msr.lastFieldKind), IReg::IRZ, imm);
        }
    }

    void MemTailCopyReg(MemSpace& ms, IReg dst, uint16_t recType) override
    {
        FATAL("MemTailCopyReg");
    }

    void MemTailCopyInterior(MemSpace& ms, IReg dst, std::vector<uint16_t> refs) override
    {
        FATAL("MemTailCopyInterior");
    }

    void MemTailCopyInteriorArr(MemSpace& ms, IReg dst, IReg idx, std::vector<uint16_t> refs) override
    {
        FATAL("MemTailCopyInteriorArr");
    }

    void MemTailCopyStatic(MemSpace& ms, std::vector<uint16_t> refs) override
    {
        FATAL("MemTailCopyStatic");
    }

    void MemTailCopyTyped(MemSpace& ms, uint16_t ts, std::vector<uint16_t> refs) override
    {
        FATAL("MemTailCopyTyped");
    }

    void MemTailCopyHandle(MemSpace& ms, IReg base, IReg offset) override
    {
        FATAL("MemTailCopyHandle");
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

    void Fail() { failedPositions.push_back(startPosition); }

    void StopRewrite()
    {
        Fail();
        auto left = reader.End() - reader.Cursor();
        reader.Advance(left);
    }
};

static std::optional<FrameLayout> makeFrameLayout(Symlevel::Code code, Resolver& resolver)
{
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
            return std::nullopt;
        }
        auto type = typeOpt.value();
        if (type->GetKind() != CbcTypeKind::REC) {
            return std::nullopt;
        }
        auto size = type->GetFlatSize();
        if (!size.has_value()) {
            return std::nullopt;
        }
        auto typeInfo = type->GetTypeInfo();
        if (!typeInfo.has_value()) {
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

static std::vector<Interpretation::PositionalInfo> CalculatePositionalGCInfo(
    Engine::Session& session, const MethodCode& code, Emitter::Emitter const& emitter, std::vector<IsaRewriter::StatePoint> const& statePoints
)
{
    auto livenessInfo = code.GetLivenessInfo(session);

    std::vector<Interpretation::PositionalInfo> posInfo;
    posInfo.reserve(livenessInfo.size());

    std::unordered_map<ssize_t, Symlevel::LivenessInfo const&> infos;

    for (const auto& info : livenessInfo) {
        infos.insert({info.cbcPos, info});
    }

    for (auto& point : statePoints) {
        auto originalPos = point.originalPos;
        auto rewrittenPos = emitter.LabelPosition(point.label);
        auto it = infos.find(originalPos);
        if (it == infos.end()) {
            FATAL("Unknown position");
        } else if (rewrittenPos > UINT32_MAX) {
            FATAL("Position too big");
        }
        auto& info = it->second;

        posInfo.push_back({ .rewrittenPos = (uint32_t) rewrittenPos, .regMask = info.regMask, .untypedRefSlotsInfo = {} });

        posInfo.back().untypedRefSlotsInfo.reserve(info.refSlotNums.size());
        for (const auto& slotN : info.refSlotNums) {
            posInfo.back().untypedRefSlotsInfo.push_back(slotN * STACK_SLOT_SIZE);
        }
    }

    return posInfo;
}

static std::string Descriptor(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method)
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
    Engine::Identifier<Symlevel::MethodDefinition> method
)
{
    using namespace Stream;
    Emitter::Emitter emitter;
    auto frameLayout = makeFrameLayout(code, resolver);

    if (!frameLayout.has_value()) {
        FATAL("Rewriter failed: cannot make frame layout.");
    }

    auto rewriter = IsaRewriter(resolver, session, method.GetFileId(), code, *frameLayout, emitter);
    rewriter.ParseAll();

    if (!rewriter.failedPositions.empty()) {
        Interpretation::Log::preparation.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
            out << "Failed to rewrite method at positions: ";
            for (auto pos : rewriter.failedPositions) {
                out << pos << ", ";
            }
            out.NewLine();
        });
        // FIXME: use stub that throws
        FATAL("Rewriter failed: cannot rewrite code.");
    }

    auto rewrittenCode = emitter.Build(heap);

    return Interpretation::ExecBytecodeInfo {
        .code             = rewrittenCode,
        .savedIRegs       = code.UsedNonVolIRegMask(),
        .savedFRegs       = code.UsedNonVolFRegMask(),
        .untypedSlotCount = static_cast<uint16_t>(code.UntypedSlotCount()),
        .frameSize        = frameLayout->frameSize,
        .gcInfo =
            Interpretation::GcInfo {
                .positionalInfo = std::move(CalculatePositionalGCInfo(session, code, emitter, rewriter.statePoints)),
                .typedSlotsInfo = std::move((*frameLayout).typedSlotsInfo),
            },
    };
}

Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method, Memory::Heap& heap
)
{
    using namespace Stream;
    auto def = Symlevel::MethodDefinition::Resolve(session, method);
    ASSERTION(def.MethodCode().has_value(), "fuh preparation must be unreachable for methods without code");

    Resolver resolver(session, method);
    auto code = Symlevel::Reader::Read(session, def.MethodCode().value());

    Interpretation::Log::preparation.Log(Logging::Level::INFO, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));
        code.Print(session, out);
        Disasm(desc, code, &resolver);
    });

    auto res = Rewrite(session, code, resolver, heap, method);

    Interpretation::Log::preparation.Log(Logging::Level::INFO, [&](Stream::Output& out) {
        Descripted desc(out, Descriptor(session, method));

        desc.PrintFmt("bytecode: %p %zu", res.code.bytecode, res.code.bytecodeSize);
        desc.NewLine();
        desc.PrintFmt("literals: %p %zu", res.code.literals->_table, res.code.literals->_byteSize / 8);
        desc.NewLine();

        desc << res;
        Cbc::RT::Log(res.code, desc);
    });

    return res;
}

} // namespace Cbc
