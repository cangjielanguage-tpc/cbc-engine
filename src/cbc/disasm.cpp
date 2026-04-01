#include "disasm.h"
#include "isa.h"

#include <cmath>

namespace Cbc {

Disassembler::Disassembler(API::Resolver* resolver, MethodCode code, ::std::ostream& out)
    : Parser(resolver, code),
      log10size { std::max(static_cast<int>(1. + std::log10(code.CodeSize())), 1) },
      out(out)
{}

void Disassembler::DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm)
{
    // cout << "ext." << sign.Name() << ' ' << dst.Name() << ' ' << src.Name() << ' ' << imm << '\n';
    Print(stream_pos, "ext.", sign, dst, src, imm);
}

void Disassembler::DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) {}

void Disassembler::DoMov(IReg dst, IReg src, bool isReference)
{
    auto refPrefix = isReference ? ".ref " : "";
    // cout << "mov" << refPrefix << dst.Name() << ' ' << src.Name() << '\n';
    Print(stream_pos, "mov", concat, refPrefix, dst, src);
}

void Disassembler::DoMovVST(IReg dst, IReg src) {}

void Disassembler::DoMovImm(Width width, IReg dst, uint64_t imm)
{
    // cout << "movi." << width.Name() << ' ' << dst.Name() << ' ' << imm << '\n';
    Print(stream_pos, "movi.", concat, width, dst, imm);
}

void Disassembler::DoINeg(Width width, IReg dst, IReg src)
{
    // cout << "ineg." << width.Name() << ' ' << dst.Name() << ' ' << src.Name() << '\n';
    Print(stream_pos, "ineg.", concat, width, dst, src);
}

void Disassembler::DoBinary(InputCommonOpc op, Width width, IReg dst, IReg src1, IReg src2)
{
    // cout << Name(op) << '.' << width.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2.Name() << '\n';
    Print(stream_pos, op, concat, '.', concat, width, dst, src1, src2);
}

void Disassembler::DoBinaryImm(InputCommonOpc op, Width width, IReg dst, IReg src1, uint64_t src2)
{
    // cout << Name(op) << ".imm." << width.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2 << '\n';
    Print(stream_pos, op, concat, ".imm.", concat, width, dst, src1, src2);
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)
{
    // cout << Name(op) << '.' << tkind.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2.Name() << '\n';
    Print(stream_pos, op, concat, '.', concat, tkind, dst, src1, src2);
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2)
{
    // cout << Name(op) << ".imm." << tkind.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2 << '\n';
    Print(stream_pos, op, concat, ".imm.", concat, tkind, dst, src1, src2);
}

void Disassembler::DoReturn(Width width, IReg dst)
{
    // cout << "ret." << width.Name() << ' ' << dst.Name() << '\n';
    Print(stream_pos, "ret.", concat, width, dst);
    Print(concat, "");
}

void Disassembler::DoReturn(Width width, FReg dst)
{
    // cout << "fret." << width.Name() << ' ' << dst.Name() << '\n';
    Print(stream_pos, "fret.", concat, width, dst);
    Print(concat, "");
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r.Name() << ' ' << target <<
    // '\n';
    Print(stream_pos, "branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r.Name() << ' ' << target <<
    // '\n';
    Print(stream_pos, "branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r << ' ' << target << '\n';
    Print(stream_pos, "branchif.", concat, op, concat, '.', concat, width, l, r, target);
}

void Disassembler::DoJmp(int32_t target)
{
    // cout << "jmp " << target << '\n';
    Print(stream_pos, "jmp", target);
}

void Disassembler::DoCallDirect(IReg d, uint16_t methodIndex)
{
    // cout << "call.direct " << d.Name() << " id:" << methodIndex << '\n';
    Print(stream_pos, "call.direct", d, "id:", concat, methodIndex);
}

} // namespace Cbc
