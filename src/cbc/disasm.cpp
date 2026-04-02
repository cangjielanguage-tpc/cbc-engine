#include "disasm.h"
#include "isa.h"

#include <cmath>

namespace Cbc {

template <typename T, typename I = void> struct has_name : std::false_type {};

template <typename T> struct has_name<T, std::void_t<decltype(std::declval<T>().Name())>> : std::true_type {};

template <typename T> constexpr bool has_name_v = has_name<T>::value;

template <typename T, typename I = void> struct name_conformal : std::false_type {};

template <typename T> struct name_conformal<T, std::void_t<decltype(Name(std::declval<T>()))>> : std::true_type {};

template <typename T> constexpr bool name_conformal_v = name_conformal<T>::value;

constexpr char sep = ' ';
constexpr char dot = '.';
constexpr char end = '\n';

template <typename T> constexpr std::string_view NameIt(const T arg)
{
    if constexpr (has_name_v<T>) {
        return arg.Name();
    } else if constexpr (name_conformal_v<T>) {
        return Name(arg);
    } else {
        return "<cannot Name given type>";
    }
}

std::ostream& Disassembler::PrintOpcode()
{
    return out << std::setw(log10size) << (uint64_t)CurrentOffset() << std::setw(0) << ':' << sep;
}

Disassembler::Disassembler(API::Resolver* resolver, MethodCode code, std::ostream& out)
    : Parser(resolver, code),
      log10size { std::max(static_cast<int>(1. + std::log10(code.CodeSize())), 1) },
      out(out)
{}

void Disassembler::DoExtend(Sign sign, IReg dst, IReg src, uint64_t imm)
{
    // cout << "ext." << sign.Name() << ' ' << dst.Name() << ' ' << src.Name() << ' ' << imm << '\n';
    PrintOpcode() << "ext." << NameIt(sign) << sep << NameIt(dst) << sep << NameIt(src) << sep << imm << end;
}

void Disassembler::DoBFX(Sign sign, Width res_width, Width arg_width, IReg dst, IReg src, uint64_t imm) {}

void Disassembler::DoMov(IReg dst, IReg src, bool isReference)
{
    auto refPrefix = isReference ? ".ref " : "";
    // cout << "mov" << refPrefix << dst.Name() << ' ' << src.Name() << '\n';
    PrintOpcode() << "mov" << refPrefix << sep << NameIt(dst) << sep << NameIt(src) << end;
}

void Disassembler::DoMovVST(IReg dst, IReg src) {}

void Disassembler::DoMovImm(Width width, IReg dst, uint64_t imm)
{
    // cout << "movi." << width.Name() << ' ' << dst.Name() << ' ' << imm << '\n';
    PrintOpcode() << "movi." << NameIt(width) << sep << NameIt(dst) << sep << imm << end;
}

void Disassembler::DoINeg(Width width, IReg dst, IReg src)
{
    // cout << "ineg." << width.Name() << ' ' << dst.Name() << ' ' << src.Name() << '\n';
    PrintOpcode() << "ineg." << NameIt(width) << sep << NameIt(dst) << sep << NameIt(src) << end;
}

void Disassembler::DoBinary(InputCommonOpc op, Width width, IReg dst, IReg src1, IReg src2)
{
    // cout << Name(op) << '.' << width.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2.Name() << '\n';
    PrintOpcode() << NameIt(op) << dot << NameIt(width) << sep << NameIt(dst) << sep << NameIt(src1) << sep
                  << NameIt(src2) << end;
}

void Disassembler::DoBinaryImm(InputCommonOpc op, Width width, IReg dst, IReg src1, uint64_t src2)
{
    // cout << Name(op) << ".imm." << width.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2 << '\n';
    PrintOpcode() << NameIt(op) << ".imm." << NameIt(width) << sep << NameIt(dst) << sep << NameIt(src1) << sep << src2
                  << end;
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)
{
    // cout << Name(op) << '.' << tkind.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2.Name() << '\n';
    PrintOpcode() << NameIt(op) << dot << NameIt(tkind) << sep << NameIt(dst) << sep << NameIt(src1) << sep
                  << NameIt(src2) << end;
}

void Disassembler::DoBinaryFloatOp(InputFloatOpc op, CbcTypeKind tkind, FReg dst, FReg src1, uint64_t src2)
{
    // cout << Name(op) << ".imm." << tkind.Name() << ' ' << dst.Name() << ' ' << src1.Name() << ' ' << src2 << '\n';
    PrintOpcode() << NameIt(op) << ".imm." << NameIt(tkind) << sep << NameIt(dst) << sep << NameIt(src1) << sep << src2
                  << end;
}

void Disassembler::DoReturn(Width width, IReg dst)
{
    // cout << "ret." << width.Name() << ' ' << dst.Name() << '\n';
    PrintOpcode() << "ret." << NameIt(width) << sep << NameIt(dst) << end << end;
}

void Disassembler::DoReturn(Width width, FReg dst)
{
    // cout << "fret." << width.Name() << ' ' << dst.Name() << '\n';
    PrintOpcode() << "fret." << NameIt(width) << sep << NameIt(dst) << end << end;
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, IReg l, IReg r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r.Name() << ' ' << target <<
    // '\n';
    PrintOpcode() << "branchif." << NameIt(op) << dot << NameIt(width) << sep << NameIt(l) << sep << NameIt(r) << sep
                  << target << end;
}

void Disassembler::DoBranchIf(InputCcOpc op, Width width, FReg l, FReg r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r.Name() << ' ' << target <<
    // '\n';
    PrintOpcode() << "branchif." << NameIt(op) << dot << NameIt(width) << sep << NameIt(l) << sep << NameIt(r) << sep
                  << target << end;
}

void Disassembler::DoBranchIfImm(InputCcOpc op, Width width, IReg l, uint64_t r, int32_t target)
{
    // cout << "branchif." << Name(op) << '.' << width.Name() << ' ' << l.Name() << ' ' << r << ' ' << target << '\n';
    PrintOpcode() << "branchif." << NameIt(op) << dot << NameIt(width) << sep << NameIt(l) << sep << r << sep << target
                  << end;
}

void Disassembler::DoJmp(int32_t target)
{
    // cout << "jmp " << target << '\n';
    PrintOpcode() << "jmp" << sep << target << end;
}

void Disassembler::DoCallDirect(IReg d, uint16_t methodIndex)
{
    // cout << "call.direct " << d.Name() << " id:" << methodIndex << '\n';
    PrintOpcode() << "call.direct" << sep << NameIt(d) << sep << "id:" << methodIndex << end;
}

} // namespace Cbc
