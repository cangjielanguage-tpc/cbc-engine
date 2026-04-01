#pragma once

#include "cbc/isa.h"
#include "cbc/parser.h"

namespace Cbc {

using namespace Format;

class Disassembler : public Parser {
public:
    Disassembler(API::Resolver* resolver, MethodCode code);

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

    void DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, int32_t target) override;
    void DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, int32_t target) override;
    void DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, int32_t target) override;

    void DoJmp(int32_t target) override;

    void DoCallDirect(IReg d, uint16_t methodIndex) override;

private:
    template<typename T>
    constexpr void PrintIt(const T arg, const char head_delim = '\0', const char tail_delim = '\0');

    template<typename T, typename... Ts>
    constexpr void PrintConcat(const T arg, const Ts... tail);

    template<typename T, typename... Ts>
    constexpr void Print(const T arg, const Ts... tail);

private:
    int log10size;
};

} // namespace Cbc

#include "disasm.ipp"
