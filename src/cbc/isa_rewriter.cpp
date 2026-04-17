#include "isa_rewriter.h"
#include "api/resolver.h"
#include "api/type.h"
#include "cbc/emitter/emitter.h"
#include "cbc/isa.h"
#include "engine/symlevel/index.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/terms.h"
#include "utils/assertion.h"
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

    void NewArr(IReg dst, IReg len, uint16_t type) override { ASSERTION(false, "not implemented"); }

    void GcPoint() override { ASSERTION(false, "not implemented"); }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override { ASSERTION(false, "not implemented"); }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override { ASSERTION(false, "not implemented"); }

    void NewObj(IReg dst, uint16_t typeIdx) override
    {
        auto type        = resolver.Resolve(Term(typeIdx));
        auto typeInfoOpt = type->GetTypeInfo();

        ASSERTION(typeInfoOpt.has_value(), "Cannot find type info for newobj");
        void* typeInfo = typeInfoOpt.value(); // get raw value

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

    void CallInterf(IReg dst, uint16_t method) override { ASSERTION(false, "not implemented"); }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        if (cc.IsFloatingPoint()) {
            // FIXME: support for floats
            ASSERTION(false, "not implemented");
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

    void DivCheck(IReg reg) override { ASSERTION(false, "not implemented"); }

    void Catch(IReg reg) override { ASSERTION(false, "not implemented"); }

    void Throw(IReg reg) override { ASSERTION(false, "not implemented"); }

    void ZeroRefs(uint16_t ts) override { ASSERTION(false, "not implemented"); }

    void InstanceOf(IReg dst, IReg obj, uint16_t type) override { ASSERTION(false, "not implemented"); }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { ASSERTION(false, "not implemented"); }

    void InitObj(uint16_t ts) override { ASSERTION(false, "not implemented"); }

    void InitString(uint16_t ts, uint32_t offset) override { ASSERTION(false, "not implemented"); }

    void ArrayLength(IReg dst, IReg arr) override { ASSERTION(false, "not implemented"); }

    void ArrayIndexCheck(IReg length, IReg index) override { ASSERTION(false, "not implemented"); }

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        startPosition = position;
        emit.Bind(InstructionLabel(Pos()));
        IsaParser::ParseOne();
    }
};

std::unique_ptr<IsaParser> Rewriter(API::Resolver& resolver, MethodCode code, Emitter::Emitter& e)
{
    return std::make_unique<IsaRewriter>(resolver, code, e);
}

} // namespace Cbc
