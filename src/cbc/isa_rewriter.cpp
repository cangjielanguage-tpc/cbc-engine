#include "isa_rewriter.h"
#include "api/field.h"
#include "api/resolver.h"
#include "api/type.h"
#include "cbc/emitter/emitter.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "engine/symlevel/index.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/terms.h"
#include "interpreter/code.h"
#include "utils/assertion.h"
#include "utils/math.h"
#include "utils/ostream.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/types.h>

namespace Cbc {

using MethodIndex = Symlevel::Index<Symlevel::MethodReference>;
using FieldIndex  = Symlevel::Index<Symlevel::FieldReference>;
using TermIndex   = Symlevel::Index<Symlevel::Term>;

struct IsaRewriter : public IsaParser {
    IsaRewriter(API::Resolver& resolver, MethodCode code, Emitter::Emitter& emit)
        : IsaParser(code),
          resolver(resolver),
          emit(emit),
          startPosition(0),
          bytecodeSize(reader.End() - reader.Start())
    {}

    API::Resolver& resolver;
    Emitter::Emitter& emit;
    size_t bytecodeSize;

    size_t startPosition;
    std::unordered_map<ssize_t, Emitter::Label> instructionLabel;

    Format::LoadAccessKind typeToLoadAccessKind(Symlevel::TemplateKind typeIdentifier)
    {
        using namespace Symlevel;
        using namespace Format;

        switch (typeIdentifier) {
            case TemplateKind::U8:  return LoadAccessKind::LD_U8;
            case TemplateKind::I8:  return LoadAccessKind::LD_S8;
            case TemplateKind::U16: return LoadAccessKind::LD_U16;
            case TemplateKind::I16: return LoadAccessKind::LD_S16;
            case TemplateKind::U32:
            case TemplateKind::I32: return LoadAccessKind::LD_32;
            case TemplateKind::U64:
            case TemplateKind::I64: return LoadAccessKind::LD_64;
            case TemplateKind::F32: return LoadAccessKind::LD_F32;
            case TemplateKind::F64: return LoadAccessKind::LD_F64;

            case TemplateKind::BOOLEAN: return LoadAccessKind::LD_U8;

            case TemplateKind::TYPE:
            case TemplateKind::AOT_TYPE:
            case TemplateKind::NULLABLE:
            case TemplateKind::NON_NULLABLE:
            case TemplateKind::CANGJIE_ARRAY: return LoadAccessKind::LD_REF;

            case TemplateKind::UADDR:
            case TemplateKind::IADDR:
            case TemplateKind::BSTRING:
            case TemplateKind::C_POINTER: return LoadAccessKind::LD_64;

            case TemplateKind::UCHAR32: return LoadAccessKind::LD_32;

            case TemplateKind::F16: return LoadAccessKind::LD_U16;

            default: {
                FATAL("Not supported template kind");
                return LoadAccessKind::LD_S8;
            }
        }
    }

    Format::StoreAccessKind typeToStoreAccessKind(Symlevel::TemplateKind typeIdentifier)
    {
        using namespace Symlevel;
        using namespace Format;

        switch (typeIdentifier) {
            case TemplateKind::U8:
            case TemplateKind::I8:  return StoreAccessKind::ST_8;
            case TemplateKind::U16:
            case TemplateKind::I16: return StoreAccessKind::ST_16;
            case TemplateKind::U32:
            case TemplateKind::I32: return StoreAccessKind::ST_32;
            case TemplateKind::U64:
            case TemplateKind::I64: return StoreAccessKind::ST_64;
            case TemplateKind::F32: return StoreAccessKind::ST_F32;
            case TemplateKind::F64: return StoreAccessKind::ST_F64;

            case TemplateKind::BOOLEAN: return StoreAccessKind::ST_8;

            case TemplateKind::TYPE:
            case TemplateKind::AOT_TYPE:
            case TemplateKind::NULLABLE:
            case TemplateKind::NON_NULLABLE:
            case TemplateKind::CANGJIE_ARRAY: return StoreAccessKind::ST_REF;

            case TemplateKind::UADDR:
            case TemplateKind::IADDR:
            case TemplateKind::BSTRING:
            case TemplateKind::C_POINTER: return StoreAccessKind::ST_64;

            case TemplateKind::UCHAR32: return StoreAccessKind::ST_32;

            case TemplateKind::F16: return StoreAccessKind::ST_16;

            default: {
                FATAL("Not supported template kind");
                return StoreAccessKind::ST_8;
            }
        }
    }

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

    TermIndex Term(uint16_t index) { return TermIndex { .region = 0, .index = index }; }

    MethodIndex Method(uint16_t index) { return MethodIndex { .region = 0, .index = index }; }

    FieldIndex Field(uint16_t index) { return FieldIndex { .region = 0, .index = index }; }

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

    // TODO: add enum
    void FloatBinary(uint8_t op, Format::Width width, FReg d, FReg l, FReg r) override {}

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        emit.Convert(toType, fromType, to, from);
    }

    void PrepareRecord(uint16_t ts) override {}

    void NewArr(IReg dst, IReg len, uint16_t type) override { FATAL("not implemented"); }

    void GcPoint() override { FATAL("not implemented"); }

    void LoadStatic(AnyReg r, uint16_t field) override
    {
        API::StaticField* resolvedField = resolver.ResolveStaticField(Field(field));

        auto fieldTerm       = resolvedField->FieldType().value()->AsTerm();
        auto fieldAccessKind = fieldTerm->GetIdentifier().GetKind();
        auto symbol          = emit.NewAddressSym(resolvedField->Location());
        emit.LoadStatic(typeToLoadAccessKind(fieldAccessKind), r, symbol);
    }

    void StoreStatic(AnyReg r, uint16_t field) override
    {
        API::StaticField* resolvedField = resolver.ResolveStaticField(Field(field));

        auto fieldTerm       = resolvedField->FieldType().value()->AsTerm();
        auto fieldAccessKind = fieldTerm->GetIdentifier().GetKind();
        auto symbol          = emit.NewAddressSym(resolvedField->Location());
        emit.StoreStatic(typeToStoreAccessKind(fieldAccessKind), r, symbol);
    }

    void LoadObj(IReg rb, AnyReg rd, uint16_t field) override
    {
        API::InstanceField* resolvedField = resolver.ResolveInstanceField(Field(field));

        auto fieldTerm       = resolvedField->FieldType().value()->AsTerm();
        auto fieldAccessKind = fieldTerm->GetIdentifier().GetKind();
        emit.LoadObj(typeToLoadAccessKind(fieldAccessKind), rd, rb, resolvedField->Offset().value());
    }

    void StoreObj(IReg rb, AnyReg rs, uint16_t field) override
    {
        API::InstanceField* resolvedField = resolver.ResolveInstanceField(Field(field));

        auto fieldTerm       = resolvedField->FieldType().value()->AsTerm();
        auto fieldAccessKind = fieldTerm->GetIdentifier().GetKind();
        emit.StoreObj(typeToStoreAccessKind(fieldAccessKind), rs, rb, resolvedField->Offset().value());
    }

    void LoadRec(IReg rb, AnyReg rs, uint16_t field) override { FATAL("not implemented"); }

    void StoreRec(IReg rb, AnyReg rd, uint16_t field) override { FATAL("not implemented"); }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override { FATAL("not implemented"); }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override { FATAL("not implemented"); }

    void NewObj(IReg dst, uint16_t typeIdx) override
    {
        auto type        = resolver.Resolve(Term(typeIdx));
        auto typeInfoOpt = type->GetTypeInfo();

        ASSERTION(typeInfoOpt.has_value(), "Cannot find type info for newobj");
        void* typeInfo = typeInfoOpt.value().Raw(); // get raw value

        auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(typeInfo));
        emit.NewObj(dst, sym);
    }

    void CallDirect(IReg dst, uint16_t method) override
    {
        auto m   = resolver.ResolveDirectMethod(Method(method));
        auto fuh = m->FUH();
        if (fuh.has_value()) {
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(fuh.value()));
            emit.DirectCall2i(sym);
        } else {
            void* target = m->TargetAddr();
            ASSERT(target != nullptr);
            auto sym = emit.NewAddressSym(reinterpret_cast<uintptr_t>(target));
            emit.DirectCall2c(sym);
        }
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
    }

    void CallVirtual(IReg dst, uint16_t method) override
    {
        auto m = resolver.ResolveVirtualMethod(Method(method));
        emit.VirtualCall2c(m->VNum(), m->ExtDefNum());
        if (dst != IReg::IR1) {
            emit.Mov(dst, IReg::IR1);
        }
    }

    void CallInterf(IReg dst, uint16_t method) override { FATAL("not implemented"); }

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

    void Ret(Format::Width width, IReg dst) override
    {
        // FIXME: encode it as one instruction
        emit.Mov(IReg::IR1, dst);
        emit.Ret();
    }

    void FRet(Format::Width width, FReg dst) override
    {
        // FIXME: encode it as one instruction
        emit.Ret();
    }

    void DivCheck(IReg reg) override { FATAL("not implemented"); }

    void Catch(IReg reg) override { FATAL("not implemented"); }

    void Throw(IReg reg) override { FATAL("not implemented"); }

    void ZeroRefs(uint16_t ts) override { FATAL("not implemented"); }

    void InstanceOf(IReg dst, IReg obj, uint16_t type) override { FATAL("not implemented"); }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { FATAL("not implemented"); }

    void InitObj(uint16_t ts) override { FATAL("not implemented"); }

    void InitString(uint16_t ts, uint32_t offset) override { FATAL("not implemented"); }

    void ArrayLength(IReg dst, IReg arr) override { FATAL("not implemented"); }

    void ArrayIndexCheck(IReg length, IReg index) override { FATAL("not implemented"); }

    uint32_t UntypedSlotOffset(uint16_t us) { return us * STACK_SLOT_SIZE; }

    void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us) override
    {
        emit.LoadFrame(ldk, Format::Reg(dst), UntypedSlotOffset(us));
    }

    void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) override
    {
        emit.StoreFrame(stk, Format::Reg(src), UntypedSlotOffset(us));
    }

    void StoreUntypedImm(uint64_t imm, uint16_t us) override
    {
        emit.StoreFrameImm(Format::StoreAccessKind::ST_64, imm, UntypedSlotOffset(us));
    }

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        startPosition = position;
        emit.Bind(InstructionLabel(Pos()));
        IsaParser::ParseOne();
    }
};

static std::unique_ptr<IsaParser> Rewriter(API::Resolver& resolver, MethodCode code, Emitter::Emitter& e)
{
    return std::make_unique<IsaRewriter>(resolver, code, e);
}

static uint32_t CalcFrameSize(Symlevel::Code code)
{
    auto stackAllocSize = Cbc::STACK_SLOT_SIZE * (code.UntypedSlotCount()); // TODO: typed stack slots
    return MathUtils::AlignUp(stackAllocSize, Cbc::FRAME_ALIGNMENT);
}

Interpretation::ExecBytecodeInfo Rewrite(MethodCode code, API::Resolver& resolver, Memory::Heap& heap)
{
    if (IsDisasmEnabled()) {
        Disasm(Stream::Disasm::isa, code, &resolver)->ParseAll();
    }

    Emitter::Emitter emitter;
    Rewriter(resolver, code, emitter)->ParseAll();

    auto rewrittenCode = emitter.Build(heap);
    auto frameSize     = CalcFrameSize(code);

    return Interpretation::ExecBytecodeInfo {
        .code             = rewrittenCode,
        .savedIRegs       = code.UsedNonVolIRegMask(),
        .savedFRegs       = code.UsedNonVolFRegMask(),
        .untypedSlotCount = static_cast<uint16_t>(code.UntypedSlotCount()),
        .frameSize        = frameSize,
        // TODO: initialize rest
    };
}

} // namespace Cbc
