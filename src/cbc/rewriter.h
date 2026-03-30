#pragma once

#include "api/resolver.h"
#include "cbc/emitter/emitter.h"
#include "cbc/isa.h"
#include "cbc/parser.h"

namespace Cbc {

using namespace Format;

class Rewriter : public Parser {
public:
    Rewriter(API::Resolver* resolver, MethodCode code, Emitter::Emitter& e) : Parser(resolver, code), e(e) {}

protected:
    void DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm) override;
    void DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) override;

    void DoMov(IReg dst, IReg src, bool isReference) override;
    void DoMovVST(IReg dst, IReg src) override;
    void DoMovImm(Width width, IReg dst, uint64_t imm) override;

    void DoINeg(Width width, IReg dst, IReg src) override;

    void DoBinary(InputCommonOpc op, Width width, IReg dst, IReg src1, IReg src2) override;
    void DoBinaryImm(InputCommonOpc op, Width width, IReg dst, IReg src1, uint64_t src2) override;

    void DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2) override;
    void DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) override;

    void DoReturn(Width width, IReg dst) override;
    void DoReturn(Width width, FReg dst) override;

    void DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, uint8_t* target) override;
    void DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, uint8_t* target) override;
    void DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, uint8_t* target) override;

    void DoJmp(uint8_t* target) override;

    void DoCallDirect(IReg d, uint16_t methodIndex) override;

private:
    void BeforeInterpretOne(uint8_t* position) override;
    Emitter::Label InstructionLabel(uint8_t* position);

    Emitter::Emitter& e;
    std::unordered_map<uint8_t*, Emitter::Label> instructionLabel;
};

} // namespace Cbc
