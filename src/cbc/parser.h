#pragma once

#include "api/resolver.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "engine/symlevel/code.h"

namespace Cbc {

using namespace Format;

using MethodCode = Symlevel::Code;

class Parser {
public:
    Parser(API::Resolver* resolver, MethodCode code); // TODO: get method's code from Method object
    void Interpret();

protected:
    virtual void BeforeInterpretOne(uint8_t* position) {}

    virtual void DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm)                                = 0;
    virtual void DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) = 0;

    virtual void DoMov(Width width, IReg dst, IReg src, bool isReference) = 0;
    virtual void DoMovVST(IReg dst, IReg src)                             = 0;
    virtual void DoMovImm(Width width, IReg dst, uint64_t imm)            = 0;

    virtual void DoINeg(CbcTypeKind tkind, IReg dst, IReg src) = 0;

    virtual void DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)     = 0;
    virtual void DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)     = 0;
    virtual void DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)     = 0;
    virtual void DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) = 0;

    virtual void DoReturn(Width width, IReg dst) = 0;
    virtual void DoReturn(Width width, FReg dst) = 0;

    virtual void DoBranchIf(CC op, Width width, IReg l, IReg r, uint8_t* target)        = 0;
    virtual void DoBranchIf(CC op, Width width, FReg l, FReg r, uint8_t* target)        = 0;
    virtual void DoBranchIfImm(CC op, Width width, IReg l, uint64_t r, uint8_t* target) = 0;

    virtual void DoCallDirect(IReg d, uint16_t methodIndex) = 0;

protected:
    API::Resolver* resolver;

private:
    void InterpretOne(uint32_t first_byte);

    void InterpretImmExt(uint64_t imm, uint32_t bits, Sign sign);

    void B2rrMov(B2rr args, Width width, bool isReference);
    void B2rrMovVST(B2rr args);
    void B2rrCommon(B2rr args, Common op, CbcTypeKind tkind);
    void B2rrSub(B2rr args, CbcTypeKind tkind);
    void B2hrMov(B2hr args, Width width);
    void B2hrCommon(B2hr args, Common op, CbcTypeKind tkind);
    void B2hrExtend(B2hr args, Sign sign);
    void B3xrrrCommon(B3xrrr args, Sign sign);

    void B2rrd8BranchIf(ConditionalBranch::B2rrd8 args, CC cc, Width width);
    void DoBranchIf(CC op, Width width, Reg l, Reg r, uint8_t* target);
    void B3xrrdTBranchIf(ConditionalBranch::B3xrrdT args, ConditionalBranch::B3xrrdT::T t, uint32_t page);
    ConditionalBranch::Continue B3xrrdTContinue(ConditionalBranch::B3xrrdT::T t, Imm4 d4);
    void B2xri8d8BranchIf(ConditionalBranch::B2xri8d8 args, uint32_t page);
    void B2xri16d0BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page);
    void B2xri16d16BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page);

    void B2xrOpc0100SOC(SymbolicObjectControl::B2xr args);

    Decoder::ByteReader codeReader;
    uint8_t* codeEnd;
    Immediate::Decoding immDecoder;
};

} // namespace Cbc
