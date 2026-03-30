#include "disasm.h"
#include "isa.h"

#include <cmath>

namespace Cbc {

Disassembler::Disassembler(API::Resolver* resolver, MethodCode code) : Parser(resolver, code), log10size{std::max(static_cast<int>(1.0 + std::log10(code.CodeSize())), 1)} {}

void Disassembler::DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm)
{
    // cout << "ext." << sign.name() << ' ' << dst.name() << ' ' << src.name() << ' ' << imm << '\n';
    print("ext.", sign, dst, src, imm);
}

void Disassembler::DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm)
{

}

void Disassembler::DoMov(IReg dst, IReg src, bool isReference)
{
    auto refPrefix = isReference ? ".ref " : " ";
    // cout << "mov" << refPrefix << dst.name() << ' ' << src.name() << '\n';
    print("mov", refPrefix, dst, src);
}

void Disassembler::DoMovVST(IReg dst, IReg src)
{

}

void Disassembler::DoMovImm(Width width, IReg dst, uint64_t imm)
{
    // cout << "movi." << width.name() << ' ' << dst.name() << ' ' << imm << '\n';
    print("movi.", width, dst, imm);
}

void Disassembler::DoINeg(Width width, IReg dst, IReg src)
{
    // cout << "ineg." << width.name() << ' ' << dst.name() << ' ' << src.name() << '\n';
    print("ineg.", width, dst, src);
}

void Disassembler::DoBinary(InputCommonOpc op, Width width, IReg dst, IReg src1, IReg src2)
{
    // cout << name(op) << '.' << width.name() << ' ' << dst.name() << ' ' << src1.name() << ' ' << src2.name() << '\n';
    print(op, width, dst, src1, src2);
}

void Disassembler::DoBinaryImm(InputCommonOpc op, Width width, IReg dst, IReg src1, uint64_t src2)
{
    // cout << name(op) << ".imm." << width.name() << ' ' << dst.name() << ' ' << src1.name() << ' ' << src2 << '\n';
    print(op, width, dst, src1, src2);
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)
{
    // cout << name(op) << '.' << tkind.name() << ' ' << dst.name() << ' ' << src1.name() << ' ' << src2.name() << '\n';
    print(op, tkind, dst, src1, src2);
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2)
{
    // cout << name(op) << ".imm." << tkind.name() << ' ' << dst.name() << ' ' << src1.name() << ' ' << src2 << '\n';
    print(op, tkind, dst, src1, src2);
}

void Disassembler::DoReturn(Width width, IReg dst)
{
    // cout << "ret." << width.name() << ' ' << dst.name() << '\n';
    print("ret.", width, dst);
}

void Disassembler::DoReturn(Width width, FReg dst)
{
    // cout << "fret." << width.name() << ' ' << dst.name() << '\n';
    print("fret.", width, dst);
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, int32_t target)
{
    // cout << "branchif." << name(op) << '.' << width.name() << ' ' << l.name() << ' ' << r.name() << ' ' << target << '\n';
    print("branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, int32_t target)
{
    // cout << "branchif." << name(op) << '.' << width.name() << ' ' << l.name() << ' ' << r.name() << ' ' << target << '\n';
    print("branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, int32_t target)
{
    // cout << "branchif." << name(op) << '.' << width.name() << ' ' << l.name() << ' ' << r << ' ' << target << '\n';
    print("branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoJmp(int32_t target)
{
    // cout << "jmp " << target << '\n';
    print("jmp", target);
}

void Disassembler::DoCallDirect(IReg d, uint16_t methodIndex)
{
    // cout << "call.direct " << d.name() << " id:" << methodIndex << '\n';
    print("jcall.direct", d, "id:", methodIndex);
}

} // namespace Cbc
