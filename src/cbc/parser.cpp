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
      codeReader(ReaderOf(code)),
      codeEnd(GetCodeEnd(code))
{}

void Parser::Interpret()
{
    while (!codeReader.EndOfMem(codeEnd)) {
        BeforeInterpretOne(codeReader.Cursor());
        InterpretOne(codeReader.PeekOpcode());
    }
}

void Parser::InterpretOne(uint32_t opcode)
{
    switch(opcode) {
    case static_cast<opcode_t>(opcode::Mov32): { decode<opcode::Mov32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mov64): { decode<opcode::Mov64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mov32i): { decode<opcode::Mov32i, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mov64i): { decode<opcode::Mov64i, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::MovVst): { decode<opcode::MovVst, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::MovRef): { decode<opcode::MovRef, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::ExtendSigned): { decode<opcode::ExtendSigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::ExtendUnsigned): { decode<opcode::ExtendUnsigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Add32): { decode<opcode::Add32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Sub32): { decode<opcode::Sub32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mul32): { decode<opcode::Mul32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::And32): { decode<opcode::And32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Or32): { decode<opcode::Or32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Xor32): { decode<opcode::Xor32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div32Signed): { decode<opcode::Div32Signed, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem32Signed): { decode<opcode::Rem32Signed, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div32Unsigned): { decode<opcode::Div32Unsigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem32Unsigned): { decode<opcode::Rem32Unsigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsr32): { decode<opcode::Lsr32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Asr32): { decode<opcode::Asr32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsl32): { decode<opcode::Lsl32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Neg32): { decode<opcode::Neg32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Add64): { decode<opcode::Add64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Sub64): { decode<opcode::Sub64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mul64): { decode<opcode::Mul64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::And64): { decode<opcode::And64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Or64): { decode<opcode::Or64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Xor64): { decode<opcode::Xor64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div64Signed): { decode<opcode::Div64Signed, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem64Signed): { decode<opcode::Rem64Signed, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div64Unsigned): { decode<opcode::Div64Unsigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem64Unsigned): { decode<opcode::Rem64Unsigned, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsr64): { decode<opcode::Lsr64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Asr64): { decode<opcode::Asr64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsl64): { decode<opcode::Lsl64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Neg64): { decode<opcode::Neg64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Add32Imm): { decode<opcode::Add32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Sub32Imm): { decode<opcode::Sub32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mul32Imm): { decode<opcode::Mul32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::And32Imm): { decode<opcode::And32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Or32Imm): { decode<opcode::Or32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Xor32Imm): { decode<opcode::Xor32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div32SignedImm): { decode<opcode::Div32SignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem32SignedImm): { decode<opcode::Rem32SignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div32UnsignedImm): { decode<opcode::Div32UnsignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem32UnsignedImm): { decode<opcode::Rem32UnsignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsr32Imm): { decode<opcode::Lsr32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Asr32Imm): { decode<opcode::Asr32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsl32Imm): { decode<opcode::Lsl32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Neg32Imm): { decode<opcode::Neg32Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Add64Imm): { decode<opcode::Add64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Sub64Imm): { decode<opcode::Sub64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Mul64Imm): { decode<opcode::Mul64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::And64Imm): { decode<opcode::And64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Or64Imm): { decode<opcode::Or64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Xor64Imm): { decode<opcode::Xor64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div64SignedImm): { decode<opcode::Div64SignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem64SignedImm): { decode<opcode::Rem64SignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Div64UnsignedImm): { decode<opcode::Div64UnsignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Rem64UnsignedImm): { decode<opcode::Rem64UnsignedImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsr64Imm): { decode<opcode::Lsr64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Asr64Imm): { decode<opcode::Asr64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Lsl64Imm): { decode<opcode::Lsl64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Neg64Imm): { decode<opcode::Neg64Imm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon32): { decode<opcode::IntegerCommon32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon64): { decode<opcode::IntegerCommon64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon32K0): { decode<opcode::IntegerCommon32K0, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon64K0): { decode<opcode::IntegerCommon64K0, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon32K8): { decode<opcode::IntegerCommon32K8, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon64K8): { decode<opcode::IntegerCommon64K8, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon32K16): { decode<opcode::IntegerCommon32K16, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::IntegerCommon64K16): { decode<opcode::IntegerCommon64K16, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedAdd): { decode<opcode::CheckedAdd, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedSub): { decode<opcode::CheckedSub, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedMul): { decode<opcode::CheckedMul, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedDiv): { decode<opcode::CheckedDiv, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedAddImm): { decode<opcode::CheckedAddImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedSubImm): { decode<opcode::CheckedSubImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedMulImm): { decode<opcode::CheckedMulImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::CheckedDivImm): { decode<opcode::CheckedDivImm, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::Bfx): { decode<opcode::Bfx, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::FloatCommon): { decode<opcode::FloatCommon, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::FloatMisc): { decode<opcode::FloatMisc, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::SetIf32): { decode<opcode::SetIf32, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::SetIf64): { decode<opcode::SetIf64, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::SetIf32Float): { decode<opcode::SetIf32Float, void>(codeReader); break; }
    case static_cast<opcode_t>(opcode::SetIf64Float): { decode<opcode::SetIf64Float, void>(codeReader); break; }
    default: ASSERTION(false, "Unexpected opcode"); break;
    }
}

void Parser::InterpretImmExt(uint64_t imm, uint32_t bits, Sign sign) { immDecoder.SetImmExt(imm, bits, sign); }

void Parser::B2rrMov(B2rr args, Width width, bool isReference)
{
    DoMov(args.rr.x.IR(), args.rr.y.IR(), isReference);
}

void Parser::B2rrMovVST(B2rr args) { DoMovVST(args.rr.x.IR(), args.rr.y.IR()); }

void Parser::B2rrCommon(B2rr args, Common op, CbcTypeKind tkind)
{
    DoCommonOp(op, tkind, args.rr.x.IR(), args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2rrSub(B2rr args, CbcTypeKind tkind)
{
    auto rx = args.rr.x.IR(), ry = args.rr.y.IR();
    if (rx.Raw() == IReg::IRZ) {
        DoINeg(tkind, ry, ry);
    } else {
        DoCommonOp(Common::SUB, tkind, rx, rx, ry);
    }
}

void Parser::B2hrMov(B2hr args, Width width)
{
    DoMovImm(width, args.xr.r.IR(), immDecoder.DecodeB2ri4(width, args.xr.imm));
}

void Parser::B2hrCommon(B2hr args, Common op, CbcTypeKind tkind)
{
    auto dst = args.xr.r.IR();
    Width::Value width;
    if (tkind == CbcTypeKind::I32) {
        width = Width::W32;
    } else {
        assert(tkind == CbcTypeKind::I64);
        width = Width::W64;
    }
    DoCommonOp(op, tkind, dst, dst, immDecoder.DecodeB2ri4(width, args.xr.imm));
}

void Parser::B2hrExtend(B2hr args, Sign sign)
{
    DoExtend(sign, args.xr.r.IR(), args.xr.r.IR(), args.xr.imm); // TODO: decode imm
}

void Parser::B3xrrrCommon(B3xrrr args, Sign sign)
{
    auto op    = Common(sign.ToBits().Shift(3) | (args.xr.imm >> 1));
    auto tkind = (args.xr.imm & 0b1) ? CbcTypeKind::I64 : CbcTypeKind::I32;
    DoCommonOp(op, tkind, args.rr.x.IR(), args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2rrd8BranchIf(ConditionalBranch::B2rrd8 args, CC cc, Width width)
{
    int32_t offset  = static_cast<int32_t>(MathUtils::SignExtend(static_cast<uint32_t>(args.imm.imm), 8));
    uint8_t* target = codeReader.Cursor() + offset;
    DoBranchIf(cc, width, args.b2rr.rr.x.IR(), args.b2rr.rr.y.IR(), target);
}

void Parser::DoBranchIf(CC op, Width width, Reg l, Reg r, uint8_t* target)
{
    if (op.IsFloatingPoint()) {
        DoBranchIf(op, width, l.FR(), r.FR(), target);
    } else {
        DoBranchIf(op, width, l.IR(), r.IR(), target);
    }
}

void Parser::B3xrrdTBranchIf(ConditionalBranch::B3xrrdT args, ConditionalBranch::B3xrrdT::T t, uint32_t page)
{
    auto cc      = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
    auto width   = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;
    auto decoded = B3xrrdTContinue(t, args.rt4.imm4);
    auto target  = codeReader.Cursor() + decoded.offset;
    DoBranchIf(cc.Negated(decoded.negated), width, args.xr.r, args.rt4.r, target);
}

ConditionalBranch::Continue Parser::B3xrrdTContinue(ConditionalBranch::B3xrrdT::T t, Imm4 d4)
{
    switch (t) {
        case ConditionalBranch::B3xrrdT::T::T0: return ConditionalBranch::C1dM::Decode(codeReader).ToContinue();
        case ConditionalBranch::B3xrrdT::T::T8: {
            uint32_t d8     = codeReader.Read8();
            uint32_t offset = MathUtils::SignExtend((offset << 4) | d4, 12);
            return ConditionalBranch::Continue { offset, 0 };
        }
        case ConditionalBranch::B3xrrdT::T::T16: {
            uint32_t d16    = codeReader.Read16();
            uint32_t offset = MathUtils::SignExtend((offset << 4) | d4, 20);
            return ConditionalBranch::Continue { offset, 0 };
        }

        default: ASSERTION(false, "Unexpected T for B3xrrdT format"); return ConditionalBranch::Continue { 0, 0 };
    }
}

void Parser::B2xri8d8BranchIf(ConditionalBranch::B2xri8d8 args, uint32_t page)
{
    auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
    assert(!cc.IsFloatingPoint());
    auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;

    auto immVal = cc.IsSigned() ? MathUtils::SignExtend(static_cast<uint32_t>(args.imm), 8)
                                : MathUtils::ZeroExtend(static_cast<uint32_t>(args.imm), 8);
    auto target = codeReader.Cursor() + MathUtils::SignExtend(static_cast<uint32_t>(args.dist), 8);

    DoBranchIfImm(cc, width, args.xr.r.IR(), immVal, target);
}

void Parser::B2xri16d0BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page)
{
    auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
    assert(!cc.IsFloatingPoint());
    auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;

    auto immVal  = immDecoder.DecodeIntegralBCCi16(cc.IsSigned() ? Sign::SIGNED : Sign::UNSIGNED, width, args.imm);
    auto decoded = ConditionalBranch::C1dM::Decode(codeReader).ToContinue();
    auto target  = codeReader.Cursor() + decoded.offset;

    DoBranchIfImm(cc.Negated(decoded.negated), width, args.xr.r.IR(), immVal, target);
}

void Parser::B2xri16d16BranchIf(ConditionalBranch::B2xri16dM args, uint32_t page)
{
    auto cc = CC((Bits(page).In(1).Shift(3) | Bits(args.xr.imm >> 1).In(3)).Raw());
    assert(!cc.IsFloatingPoint());
    auto width = (args.xr.imm & 0b1) == 0 ? Width::W32 : Width::W64;

    auto immVal = immDecoder.DecodeIntegralBCCi16(cc.IsSigned() ? Sign::SIGNED : Sign::UNSIGNED, width, args.imm);
    auto offset = MathUtils::SignExtend(static_cast<uint32_t>(Imm16::Decode(codeReader)), 16);
    auto target = codeReader.Cursor() + offset;

    DoBranchIfImm(cc, width, args.xr.r.IR(), immVal, target);
}

void Parser::B2xrOpc0100SOC(SymbolicObjectControl::B2xr args)
{
    switch (SymbolicObjectControl::Opc0100(args.xr.imm)) {
        case SymbolicObjectControl::Opc0100::RET_32:  DoReturn(Width::W32, args.xr.r.IR()); break;
        case SymbolicObjectControl::Opc0100::RET_64:  DoReturn(Width::W64, args.xr.r.IR()); break;
        case SymbolicObjectControl::Opc0100::FRET_32: DoReturn(Width::W32, args.xr.r.FR()); break;
        case SymbolicObjectControl::Opc0100::FRET_64: DoReturn(Width::W64, args.xr.r.FR()); break;

        default: ASSERTION(false, "Not implemented"); break;
    }
}

void Parser::B2xrOpc1000SOC(SymbolicObjectControl::B2xrI args)
{
    switch (SymbolicObjectControl::Opc1000(args.xr.imm)) {
        case SymbolicObjectControl::Opc1000::CALL_DIRECT: DoCallDirect(args.xr.r.IR(), args.imm.imm); break;

        default: ASSERTION(false, "Not implemented"); break;
    }
}

} // namespace Cb
