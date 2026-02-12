#pragma once

#include "api/method.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

using namespace Format;

class Parser {

public:
    Parser(API::Method* _method, Decoder::ByteReader& _stream) : method(_method), stream(_stream) {}
    void Interpret();

protected:
    void InterpretOne(uint32_t first_byte);

    virtual void doExtend(Sign sign, IReg dst, IReg src, uint64_t imm) = 0;
    virtual void doBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) = 0;

    virtual void doMov(Width width, IReg dst, IReg src, bool is_reference) = 0;
    virtual void doMovVST(IReg dst, IReg src) = 0;
    virtual void doMovImm(Width width, IReg dst, uint64_t imm) = 0;

    virtual void doINeg(CbcTypeKind tkind, IReg dst, IReg src) = 0;

    virtual void doCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) = 0;
    virtual void doCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void doCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) = 0;
    virtual void doCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void doBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2) = 0;
    virtual void doBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) = 0;

    virtual void doReturn(Width width, IReg dst) = 0;
    virtual void doReturn(Width width, FReg dst) = 0;

private:
    void B2rrMov(B2rr args, Width width, bool is_reference);
    void B2rrMovVST(B2rr args);
    void B2rrCommon(B2rr args, Common op, CbcTypeKind tkind);
    void B2rrSub(B2rr args, CbcTypeKind tkind);
    void B2hrMov(B2hr args, Width width);
    void B2hrCommon(B2hr args, Common op, CbcTypeKind tkind);
    void B2hrExtend(B2hr args, Sign sign);
    void B3xrrrCommon(B3xrrr args, Sign sign);
    void B2xrOpc0100SOC(SymbolicObjectControl::B2xr args);

    API::Method* method;
    Decoder::ByteReader& stream;
};
    
} // namespace Cbc