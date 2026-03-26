#pragma once

#include <type_traits>

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

    virtual void DoMov(IReg dst, IReg src, bool isReference)   = 0;
    virtual void DoMovVST(IReg dst, IReg src)                  = 0;
    virtual void DoMovImm(Width width, IReg dst, uint64_t imm) = 0;

    virtual void DoINeg(CbcTypeKind tkind, IReg dst, IReg src)     = 0;
    virtual void DoINeg(CbcTypeKind tkind, IReg dst, uint64_t imm) = 0;

    virtual void DoCommonOp(InputCommonOpc op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)     = 0;
    virtual void DoCommonOp(InputCommonOpc op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoCheckedOp(InputCheckedOpc op, CbcTypeKind tkind, IReg dst, IReg src1, IReg src2)     = 0;
    virtual void DoCheckedOp(InputCheckedOpc op, CbcTypeKind tkind, IReg dst, IReg src1, uint64_t src2) = 0;

    virtual void DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)     = 0;
    virtual void DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2) = 0;

    virtual void DoReturn(Width width, IReg dst) = 0;
    virtual void DoReturn(Width width, FReg dst) = 0;

    virtual void DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, uint8_t* target)        = 0;
    virtual void DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, uint8_t* target)        = 0;
    virtual void DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, uint8_t* target) = 0;

    virtual void DoCallDirect(IReg d, uint16_t methodIndex) = 0;

protected:
    API::Resolver* resolver;
    InputImmPrefix immPrefix;

private:
    void InterpretOne(uint32_t first_byte);

    template <InputOpcode opcode> inline void Decode(_with_valid_def<opcode>& codeReader) {}

    template <InputOpcode opcode> inline void Decode(...)
    {
        static_assert(false, "[ERR] Attempt to implement decoder for unknown InputOpcode!");
    }

    Decoder::ByteReader codeReader;
    uint8_t* codeEnd;
};

} // namespace Cbc
