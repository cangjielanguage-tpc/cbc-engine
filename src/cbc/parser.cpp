#include "cbc/parser.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

static uint8_t* GetCodeEnd(MethodCode code) { return code.CodePtr() + code.CodeSize(); }

static Decoder::ByteReader ReaderOf(MethodCode code)
{
    return Decoder::ByteReader(code.CodePtr(), code.CodePtr(), GetCodeEnd(code));
}

Parser::Parser(API::Resolver* resolver, MethodCode code)
    : resolver(resolver),
      immPrefix(ImmPrefix::__LAST),
      codeReader(ReaderOf(code)),
      codeEnd(GetCodeEnd(code))
{}

void Parser::Interpret()
{
    while (!codeReader.EndOfMem(codeEnd)) {
        BeforeInterpretOne(codeReader.Cursor());
        InterpretOne(codeReader.Read8());
    }
}

// #include "isa_opcode_def.h"
// GEN_JUMP_TABLE(DO_WITH_ALL_OPCODES(GEN_JUMP_TABLE_ENTRY))
// #include "isa_opcode_undef.h"

inline void ::Cbc::Parser::InterpretOne(uint32_t opcode)
{
    switch (opcode) {
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov32i): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32i>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mov64i): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64i>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::MovVst): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovVst>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::MovRef): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovRef>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ExtendSigned): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendSigned>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ExtendUnsigned): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendUnsigned>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Add64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Sub64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Mul64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::And64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Or64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Xor64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivSigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemSigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::DivUnsigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::RemUnsigned64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsr64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Asr64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Lsl64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg32Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Neg64Imm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64Imm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon32K16): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32K16>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::IntegerCommon64K16): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64K16>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedAdd): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAdd>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedSub): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSub>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedMul): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMul>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedDiv): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDiv>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedAddImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAddImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedSubImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSubImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedMulImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMulImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::CheckedDivImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDivImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Bfx): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Bfx>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatCommon): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommon>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatCommonImm): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommonImm>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::FloatMisc): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatMisc>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::SetIf32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::SetIf64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::SetIf32Float): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf32Float>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::SetIf64Float): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf64Float>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret32F): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32F>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::Ret64F): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64F>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ImmPrefix32): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix32>(codeReader); break; }
    case ::Cbc::Opc(::Cbc::InputOpcode::ImmPrefix64): { ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix64>(codeReader); break; }
    default: ASSERTION(false, "Unexpected opcode"); break;
    }
}

// void Parser::InterpretImmExt(uint64_t imm, uint32_t bits, Sign sign) { immDecoder.SetImmExt(imm, bits, sign); }
//
// void Parser::B2rrd8BranchIf(ConditionalBranch::B2rrd8 args, CC cc, Width width)
// {
//     int32_t offset  = static_cast<int32_t>(MathUtils::SignExtend(static_cast<uint32_t>(args.imm.imm), 8));
//     uint8_t* target = codeReader.Cursor() + offset;
//     DoBranchIf(cc, width, args.b2rr.rr.x.IR(), args.b2rr.rr.y.IR(), target);
// }
//
// void Parser::DoBranchIf(CC op, Width width, Reg l, Reg r, uint8_t* target)
// {
//     if (op.IsFloatingPoint()) {
//         DoBranchIf(op, width, l.FR(), r.FR(), target);
//     } else {
//         DoBranchIf(op, width, l.IR(), r.IR(), target);
//     }
// }
//
// void Parser::B3xrrdTBranchIf(ConditionalBranch::B3xrrdT args, ConditionalBranch::B3xrrdT::T t, uint32_t page)
// {
//     auto cc      = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
//     auto width   = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;
//     auto decoded = B3xrrdTContinue(t, args.rt4.imm4);
//     auto target  = codeReader.Cursor() + decoded.offset;
//     DoBranchIf(cc.Negated(decoded.negated), width, args.xr.r, args.rt4.r, target);
// }
//
// ConditionalBranch::Continue Parser::B3xrrdTContinue(ConditionalBranch::B3xrrdT::T t, Imm4 d4)
// {
//     switch (t) {
//         case ConditionalBranch::B3xrrdT::T::T0: return ConditionalBranch::C1dM::Decode(codeReader).ToContinue();
//         case ConditionalBranch::B3xrrdT::T::T8: {
//             uint32_t d8     = codeReader.Read8();
//             uint32_t offset = MathUtils::SignExtend((offset << 4) | d4, 12);
//             return ConditionalBranch::Continue { offset, 0 };
//         }
//         case ConditionalBranch::B3xrrdT::T::T16: {
//             uint32_t d16    = codeReader.Read16();
//             uint32_t offset = MathUtils::SignExtend((offset << 4) | d4, 20);
//             return ConditionalBranch::Continue { offset, 0 };
//         }
//
//         default: ASSERTION(false, "Unexpected T for B3xrrdT format"); return ConditionalBranch::Continue { 0, 0 };
//     }
// }
//
// void Parser::B2xri8d8BranchIf(ConditionalBranch::B2xri8d8 args, uint32_t page)
// {
//     auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
//     assert(!cc.IsFloatingPoint());
//     auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;
//
//     auto immVal = cc.IsSigned() ? MathUtils::SignExtend(static_cast<uint32_t>(args.imm), 8)
//                                 : MathUtils::ZeroExtend(static_cast<uint32_t>(args.imm), 8);
//     auto target = codeReader.Cursor() + MathUtils::SignExtend(static_cast<uint32_t>(args.dist), 8);
//
//     DoBranchIfImm(cc, width, args.xr.r.IR(), immVal, target);
// }
//
// void Parser::B2xri16d0BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page)
// {
//     auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
//     assert(!cc.IsFloatingPoint());
//     auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;
//
//     auto immVal  = immDecoder.DecodeIntegralBCCi16(cc.IsSigned() ? Sign::SIGNED : Sign::UNSIGNED, width, args.imm);
//     auto decoded = ConditionalBranch::C1dM::Decode(codeReader).ToContinue();
//     auto target  = codeReader.Cursor() + decoded.offset;
//
//     DoBranchIfImm(cc.Negated(decoded.negated), width, args.xr.r.IR(), immVal, target);
// }
//
// void Parser::B2xri16d16BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page)
// {
//     auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
//     assert(!cc.IsFloatingPoint());
//     auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;
//
//     auto immVal = immDecoder.DecodeIntegralBCCi16(cc.IsSigned() ? Sign::SIGNED : Sign::UNSIGNED, width, args.imm);
//     auto offset = MathUtils::SignExtend(static_cast<uint32_t>(Imm16::Decode(codeReader)), 16);
//     auto target = codeReader.Cursor() + offset;
//
//     DoBranchIfImm(cc, width, args.xr.r.IR(), immVal, target);
// }
//
// void Parser::B2xrOpc0100SOC(SymbolicObjectControl::B2xr args)
// {
//     switch (SymbolicObjectControl::Opc0100(args.xr.imm)) {
//         case SymbolicObjectControl::Opc0100::RET_32:  DoReturn(Width::W32, args.xr.r.IR()); break;
//         case SymbolicObjectControl::Opc0100::RET_64:  DoReturn(Width::W64, args.xr.r.IR()); break;
//         case SymbolicObjectControl::Opc0100::FRET_32: DoReturn(Width::W32, args.xr.r.FR()); break;
//         case SymbolicObjectControl::Opc0100::FRET_64: DoReturn(Width::W64, args.xr.r.FR()); break;
//
//         default: ASSERTION(false, "Not implemented"); break;
//     }
// }
//
// void Parser::B2xrOpc1000SOC(SymbolicObjectControl::B2xrI args)
// {
//     switch (SymbolicObjectControl::Opc1000(args.xr.imm)) {
//         case SymbolicObjectControl::Opc1000::CALL_DIRECT: DoCallDirect(args.xr.r.IR(), args.imm.imm); break;
//
//         default: ASSERTION(false, "Not implemented"); break;
//     }
// }

} // namespace Cbc
