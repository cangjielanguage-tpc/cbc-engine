#pragma once

#include "api/method.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

using namespace Format;

struct MethodCode { // TODO: move to Method's API
    uint8_t* codePtr;
    uint32_t codeSize;

    inline uint8_t* GetCodeEnd() { return codePtr + codeSize; }
    inline Decoder::ByteReader GetReader() { return Decoder::ByteReader(codePtr, codePtr, GetCodeEnd()); }
};

class Parser {
public:
    Parser(API::Method* _method, MethodCode _code) : // TODO: get method's code from Method object
        method(_method), codeReader(_code.GetReader()), codeEnd(_code.GetCodeEnd()) {}
    void Interpret();

protected:
    void InterpretOne(uint32_t first_byte);

    virtual void DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm) = 0;
    virtual void DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) = 0;

    virtual void DoMov(Width width, IReg dst, IReg src, bool isReference) = 0;
    virtual void DoMovVST(IReg dst, IReg src) = 0;
    virtual void DoMovImm(Width width, IReg dst, uint64_t imm) = 0;

    virtual void DoINeg(CbcTypeKind tkind, IReg dst, IReg src) = 0;

    virtual void DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) = 0;
    virtual void DoCommonOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2) = 0;
    virtual void DoCheckedOp(Common op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2) = 0;
    virtual void DoBinaryFloatOp(Common op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) = 0;

    virtual void DoReturn(Width width, IReg dst) = 0;
    virtual void DoReturn(Width width, FReg dst) = 0;

private:
    void B2rrMov(B2rr args, Width width, bool isReference);
    void B2rrMovVST(B2rr args);
    void B2rrCommon(B2rr args, Common op, CbcTypeKind tkind);
    void B2rrSub(B2rr args, CbcTypeKind tkind);
    void B2hrMov(B2hr args, Width width);
    void B2hrCommon(B2hr args, Common op, CbcTypeKind tkind);
    void B2hrExtend(B2hr args, Sign sign);
    void B3xrrrCommon(B3xrrr args, Sign sign);
    void B2xrOpc0100SOC(SymbolicObjectControl::B2xr args);

    API::Method* method;
    Decoder::ByteReader codeReader;
    uint8_t* codeEnd;
};
    
} // namespace Cbc