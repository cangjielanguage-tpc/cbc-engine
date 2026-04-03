#include "isa_rewriter.h"
#include "api/resolver.h"
#include "cbc/emitter/emitter.h"
#include "cbc/isa.h"
#include "utils/assertion.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/types.h>

namespace Cbc {

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

    // TODO: add enum
    void Cast(int8_t fromType, int8_t toType, AnyReg d, AnyReg s) override {}

    void PrepareRecord(uint16_t ts) override {}

    void NewArr(IReg dst, IReg len, uint16_t type) override { ASSERTION(false, "not implemented"); }

    void GcPoint() override { ASSERTION(false, "not implemented"); }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override { ASSERTION(false, "not implemented"); }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override { ASSERTION(false, "not implemented"); }

    void NewObj(IReg dst, uint16_t type) override {}

    void CallDirect(IReg dst, uint16_t method) override {}

    void CallVirtual(IReg dst, uint16_t method) override { ASSERTION(false, "not implemented"); }

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
