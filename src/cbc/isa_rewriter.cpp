#include "isa_rewriter.h"
#include "cbc/emitter/emitter.h"
#include "cbc/formater_rt.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "engine/resolving_output.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "interpreter/literals.h"
#include "interpreter/loggers.h"
#include "offsets_index.h"
#include "resolution/resolution.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/ostream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sys/types.h>
#include <variant>

namespace Cbc {

using namespace Resolution;

using TK  = CbcTypeKind;
using LDK = Format::LoadAccessKind;
using STK = Format::StoreAccessKind;

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

struct IsaRewriter : public IsaParser {
    IsaRewriter(Resolver& resolver, MethodCode code, FrameLayout frameLayout, Emitter::Emitter& emit)
        : IsaParser(code),
          resolver(resolver),
          emit(emit),
          frameLayout(frameLayout),
          startPosition(0),
          bytecodeSize(reader.End() - reader.Start())
    {}

    Resolver& resolver;
    Emitter::Emitter& emit;
    FrameLayout frameLayout;
    size_t bytecodeSize;
    Stream::Output& errStream = Interpretation::Log::preparation.Stream(Logging::Level::ERROR);

    size_t startPosition;
    std::unordered_map<ssize_t, Emitter::Label> instructionLabel;

    bool failed = false;

    InstructionOffsetsIndex BuildOffsetsIndex() { return InstructionOffsetsIndex::Create(emit, instructionLabel); }

    Emitter::Label InstructionLabel(ssize_t position)
    {
        ASSERTION(position >= 0, "label position is negative");
        ASSERTION(position < bytecodeSize, "label position is negative");
        if (auto existing = instructionLabel.find(position); existing != instructionLabel.end()) {
            return existing->second;
        } else {
            auto label = emit.NewLabel();
            instructionLabel.insert({ position, label });
            return label;
        }
    }

    ssize_t Pos()
    {
        auto start  = reader.Start();
        auto cursor = reader.Cursor();
        return cursor - start;
    }

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        if (cc.IsFloatingPoint()) {
            // FIXME: support floats
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

    void MovRef(IReg d, IReg s) override { emit.MovRef(d, s); }

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

    void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) override
    {
        // We can ignore resW and argW, because interpreter computes the result in 64-bit number anyway.
        if (sx) {
            emit.BFXS(dst, src, offset, size);
        } else {
            emit.BFXZ(dst, src, offset, size);
        }
    }

    void PrepareRecord(uint16_t ts) override {}

    void NewArr(IReg dst, IReg len, uint16_t type) override { FATAL("not implemented"); }

    void GcPoint() override { emit.GcPoint(); }

    virtual void LoadStackRec(IReg r, uint16_t ts) override
    {
        emit.LoadFrame(Format::LoadAccessKind::LEA, r, frameLayout.typedOffset[ts]);
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
            emit.StoreObj(Stk(field->fieldType->GetKind()), rs, rb, field->offset.value());
        } else {
            errStream << "Failed to get offset of field " << *field << Stream::endl;
            Fail();
        }
    }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override { FATAL("not implemented"); }

    void LoadTypeInfoSig(IReg dst, uint16_t typeId) override
    {
        auto t = resolver.Query(Index<Type>(typeId));
        if (!t.has_value()) {
            Fail();
            return;
        }
        auto typeInfo = t.value()->GetTypeInfo()->Raw();
        emit.MovImm(Format::Width::W64, dst, reinterpret_cast<uint64_t>(typeInfo));
    }

    void NewObj(IReg dst, uint16_t typeId) override
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
        emit.NewObj(typeInfo);
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
    }

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
        } else {
            auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(method->data);
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(fuh));
            emit.DirectCall2i(sym);
        }
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
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
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
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
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
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
        if (src != IReg::IR1) {
            emit.Mov(IReg::IR1, src);
        }
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
        if (src != IReg::IR1) {
            emit.Mov(IReg::IR1, src);
        }
        emit.Ret();
    }

    void DivCheck(IReg reg) override { emit.DivCheck(reg); }

    void NullCheck(IReg reg) override { emit.NullCheck(reg); }

    void Catch(IReg reg) override { FATAL("not implemented"); }

    void Throw(IReg reg) override { FATAL("not implemented"); }

    void ZeroRefs(uint16_t ts) override { FATAL("not implemented"); }

    void InstanceOf(IReg dst, IReg obj, uint16_t type) override { FATAL("not implemented"); }

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

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        startPosition = position;
        emit.Bind(InstructionLabel(Pos()));
        IsaParser::ParseOne();
    }

    void Fail() { failed = true; }

    void StopRewrite()
    {
        auto left = reader.End() - reader.Cursor();
        reader.Advance(left);
        failed = true;
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

        typedOffset.insert({ i, stackAllocSize });
        stackAllocSize += MathUtils::AlignUp(size.value(), Cbc::STACK_SLOT_SIZE);
    }

    auto frameSize = MathUtils::AlignUp(savedRegsSpace + stackAllocSize, Cbc::FRAME_ALIGNMENT);

    return FrameLayout { std::move(typedOffset), untypedSlotsSize, frameSize };
}

static std::vector<Interpretation::ReferenceInfo> CalculateReferencesMap(
    Engine::Session& session, const MethodCode& code, const InstructionOffsetsIndex& offIndex
)
{
    auto livenessInfo = code.GetLivenessInfo(session);

    std::vector<Interpretation::ReferenceInfo> refInfo;
    refInfo.reserve(livenessInfo.size());

    for (const auto& info : livenessInfo) {
        auto posOpt = offIndex.FindMappedOffset(CBC, info.cbcPos);
        if (!posOpt.has_value()) {
            FATAL("Unknown position");
        }

        refInfo.push_back({ .rewrittenPos = posOpt.value(), .regMask = info.regMask, .refSlotOffsets = {} });

        refInfo.back().refSlotOffsets.reserve(info.refSlotNums.size());
        for (const auto& slotN : info.refSlotNums) {
            refInfo.back().refSlotOffsets.push_back(slotN * STACK_SLOT_SIZE);
        }
    }

    return refInfo;
}

Interpretation::ExecBytecodeInfo Rewrite(
    Engine::Session& session, MethodCode code, Resolver& resolver, Memory::Heap& heap
)
{
    Emitter::Emitter emitter;
    auto frameLayout = makeFrameLayout(code, resolver);

    if (!frameLayout.has_value()) {
        FATAL("Rewriter failed: cannot make frame layout.");
    }

    auto rewriter = IsaRewriter(resolver, code, *frameLayout, emitter);
    rewriter.ParseAll();

    if (rewriter.failed) {
        FATAL("Rewriter failed: cannot rewrite code.");
    }

    auto offsetsIndex  = rewriter.BuildOffsetsIndex();
    auto rewrittenCode = emitter.Build(heap);

    return Interpretation::ExecBytecodeInfo {
        .code             = rewrittenCode,
        .savedIRegs       = code.UsedNonVolIRegMask(),
        .savedFRegs       = code.UsedNonVolFRegMask(),
        .untypedSlotCount = static_cast<uint16_t>(code.UntypedSlotCount()),
        .frameSize        = (*frameLayout).frameSize,
        .referenceInfos   = CalculateReferencesMap(session, code, offsetsIndex),
    };
}

static std::string Descriptor(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method)
{
    Stream::StringBuffer buf;
    Stream::ResolvingOutput out(session, buf);
    out << method << " ";
    return buf.ToString();
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

    auto res = Rewrite(session, code, resolver, heap);

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
