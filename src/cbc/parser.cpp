#include "cbc/parser.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

static uint8_t* GetCodeEnd(MethodCode code) { return code.codePtr + code.codeSize; }

static Decoder::ByteReader ReaderOf(MethodCode code)
{
    return Decoder::ByteReader(code.codePtr, code.codePtr, GetCodeEnd(code));
}

Parser::Parser(API::Method* _method, MethodCode _code)
    : method(_method),
      codeReader(ReaderOf(_code)),
      codeEnd(GetCodeEnd(_code))
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
    switch (opcode) {
        case B2rr::Fmt(Common::MOV, Width::W32):  B2rrMov(B2rr::Decode(codeReader), Width::W32, false); break;
        case B2rr::Fmt(Common::MOV, Width::W64):  B2rrMov(B2rr::Decode(codeReader), Width::W64, false); break;
        case B2rr::Fmt(Common::MVST, Width::W32): B2rrMovVST(B2rr::Decode(codeReader)); break;
        case B2rr::Fmt(Common::MREF, Width::W64): B2rrMov(B2rr::Decode(codeReader), Width::W64, true); break;

        case B2rr::Fmt(Common::ADD, Width::W32):
            B2rrCommon(B2rr::Decode(codeReader), Common::ADD, CbcTypeKind::I32);
            break;
        case B2rr::Fmt(Common::ADD, Width::W64):
            B2rrCommon(B2rr::Decode(codeReader), Common::ADD, CbcTypeKind::I64);
            break;
        case B2rr::Fmt(Common::SUB, Width::W32): B2rrSub(B2rr::Decode(codeReader), CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::SUB, Width::W64): B2rrSub(B2rr::Decode(codeReader), CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::MUL, Width::W32):
            B2rrCommon(B2rr::Decode(codeReader), Common::MUL, CbcTypeKind::I32);
            break;
        case B2rr::Fmt(Common::MUL, Width::W64):
            B2rrCommon(B2rr::Decode(codeReader), Common::MUL, CbcTypeKind::I64);
            break;
        case B2rr::Fmt(Common::AND, Width::W32):
            B2rrCommon(B2rr::Decode(codeReader), Common::AND, CbcTypeKind::I32);
            break;
        case B2rr::Fmt(Common::AND, Width::W64):
            B2rrCommon(B2rr::Decode(codeReader), Common::AND, CbcTypeKind::I64);
            break;
        case B2rr::Fmt(Common::OR, Width::W32):
            B2rrCommon(B2rr::Decode(codeReader), Common::OR, CbcTypeKind::I32);
            break;
        case B2rr::Fmt(Common::OR, Width::W64):
            B2rrCommon(B2rr::Decode(codeReader), Common::OR, CbcTypeKind::I64);
            break;
        case B2rr::Fmt(Common::XOR, Width::W32):
            B2rrCommon(B2rr::Decode(codeReader), Common::XOR, CbcTypeKind::I32);
            break;
        case B2rr::Fmt(Common::XOR, Width::W64):
            B2rrCommon(B2rr::Decode(codeReader), Common::XOR, CbcTypeKind::I64);
            break;

        case B2hr::Fmt(Common::MOV, Width::W32): B2hrMov(B2hr::Decode(codeReader), Width::W32); break;
        case B2hr::Fmt(Common::MOV, Width::W64): B2hrMov(B2hr::Decode(codeReader), Width::W64); break;
        case B2hr::Fmt(Common::ADD, Width::W32):
            B2hrCommon(B2hr::Decode(codeReader), Common::ADD, CbcTypeKind::I32);
            break;
        case B2hr::Fmt(Common::ADD, Width::W64):
            B2hrCommon(B2hr::Decode(codeReader), Common::ADD, CbcTypeKind::I64);
            break;
        case B2hr::Fmt(Common::MUL, Width::W32):
            B2hrCommon(B2hr::Decode(codeReader), Common::MUL, CbcTypeKind::I32);
            break;
        case B2hr::Fmt(Common::MUL, Width::W64):
            B2hrCommon(B2hr::Decode(codeReader), Common::MUL, CbcTypeKind::I64);
            break;
        case B2hr::Fmt(Common::AND, Width::W32):
            B2hrCommon(B2hr::Decode(codeReader), Common::AND, CbcTypeKind::I32);
            break;
        case B2hr::Fmt(Common::AND, Width::W64):
            B2hrCommon(B2hr::Decode(codeReader), Common::AND, CbcTypeKind::I64);
            break;
        case B2hr::Fmt(Common::OR, Width::W32):
            B2hrCommon(B2hr::Decode(codeReader), Common::OR, CbcTypeKind::I32);
            break;
        case B2hr::Fmt(Common::OR, Width::W64):
            B2hrCommon(B2hr::Decode(codeReader), Common::OR, CbcTypeKind::I64);
            break;
        case B2hr::Fmt(Common::XOR, Width::W32):
            B2hrCommon(B2hr::Decode(codeReader), Common::XOR, CbcTypeKind::I32);
            break;
        case B2hr::Fmt(Common::XOR, Width::W64):
            B2hrCommon(B2hr::Decode(codeReader), Common::XOR, CbcTypeKind::I64);
            break;
        case B2hr::Fmt(Common::EXT, Sign::SIGNED):   B2hrExtend(B2hr::Decode(codeReader), Sign::SIGNED); break;
        case B2hr::Fmt(Common::EXT, Sign::UNSIGNED): B2hrExtend(B2hr::Decode(codeReader), Sign::UNSIGNED); break;

        case B3xrrr::Fmt(OP7A::COMMON, Sign::SIGNED): B3xrrrCommon(B3xrrr::Decode(codeReader), Sign::SIGNED); break;

        case B1piN::Fmt(B1piN::Size::SZ_8, Sign::SIGNED):
            InterpretImmExt(Imm8::Decode(codeReader).imm, 8, Sign::SIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_16, Sign::SIGNED):
            InterpretImmExt(Imm16::Decode(codeReader).imm, 16, Sign::SIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_32, Sign::SIGNED):
            InterpretImmExt(Imm32::Decode(codeReader).imm, 32, Sign::SIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_48, Sign::SIGNED):
            InterpretImmExt(Imm48::Decode(codeReader).imm, 48, Sign::SIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_8, Sign::UNSIGNED):
            InterpretImmExt(Imm8::Decode(codeReader).imm, 8, Sign::UNSIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_16, Sign::UNSIGNED):
            InterpretImmExt(Imm16::Decode(codeReader).imm, 16, Sign::UNSIGNED);
            break;
        case B1piN::Fmt(B1piN::Size::SZ_32, Sign::UNSIGNED):
            InterpretImmExt(Imm32::Decode(codeReader).imm, 32, Sign::UNSIGNED);
            break;

        case ConditionalBranch::B2rrd8::Fmt(CC::EQ, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::EQ, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::EQ, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::EQ, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::NE, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::NE, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::NE, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::NE, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::LT, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::LT, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::LT, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::LT, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::GE, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::GE, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::GE, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::GE, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::ULT, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::ULT, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::ULT, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::ULT, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::UGE, Width::W32):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::UGE, Width::W32);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::UGE, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::UGE, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::REQ, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::REQ, Width::W64);
            break;
        case ConditionalBranch::B2rrd8::Fmt(CC::RNE, Width::W64):
            B2rrd8BranchIf(ConditionalBranch::B2rrd8::Decode(codeReader), CC::RNE, Width::W64);
            break;

        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T8, 0):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T8, 0);
        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T8, 1):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T8, 1);
        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T16, 0):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T16, 0);
        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T16, 1):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T16, 1);
        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T0, 0):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T0, 0);
        case ConditionalBranch::B3xrrdT::BranchIf(ConditionalBranch::B3xrrdT::T::T0, 1):
            B3xrrdTBranchIf(ConditionalBranch::B3xrrdT::Decode(codeReader), ConditionalBranch::B3xrrdT::T::T0, 1);

        case ConditionalBranch::B2xri8d8::Fmt(0): B2xri8d8BranchIf(ConditionalBranch::B2xri8d8::Decode(codeReader), 0);
        case ConditionalBranch::B2xri8d8::Fmt(1): B2xri8d8BranchIf(ConditionalBranch::B2xri8d8::Decode(codeReader), 1);
        case ConditionalBranch::B2xri16dM::BranchIf(ConditionalBranch::B2xri16dM::M::M0, 0):
            B2xri16d0BranchIf(ConditionalBranch::B2xri16dM::Decode(codeReader), 0);
        case ConditionalBranch::B2xri16dM::BranchIf(ConditionalBranch::B2xri16dM::M::M0, 1):
            B2xri16d0BranchIf(ConditionalBranch::B2xri16dM::Decode(codeReader), 1);
        case ConditionalBranch::B2xri16dM::BranchIf(ConditionalBranch::B2xri16dM::M::M16, 0):
            B2xri16d16BranchIf(ConditionalBranch::B2xri16dM::Decode(codeReader), 0);
        case ConditionalBranch::B2xri16dM::BranchIf(ConditionalBranch::B2xri16dM::M::M16, 1):
            B2xri16d16BranchIf(ConditionalBranch::B2xri16dM::Decode(codeReader), 1);

        case SymbolicObjectControl::Opc0100::OPCODE:
            B2xrOpc0100SOC(SymbolicObjectControl::B2xr::Decode(codeReader));
            break;

        default: ASSERTION(false, "Unexpected opcode"); break;
    }
}

void Parser::InterpretImmExt(uint64_t imm, uint32_t bits, Sign sign) { immDecoder.SetImmExt(imm, bits, sign); }

void Parser::B2rrMov(B2rr args, Width width, bool isReference)
{
    DoMov(width, args.rr.x.IR(), args.rr.y.IR(), isReference);
}

void Parser::B2rrMovVST(B2rr args) { DoMovVST(args.rr.x.IR(), args.rr.y.IR()); }

void Parser::B2rrCommon(B2rr args, Common op, CbcTypeKind tkind)
{
    DoCommonOp(op, tkind, args.rr.x.IR(), args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2rrSub(B2rr args, CbcTypeKind tkind)
{
    auto rx = args.rr.x.IR(), ry = args.rr.y.IR();
    if (rx == IReg::IRZ) {
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
    int32_t offset  = args.imm.imm;
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

} // namespace Cbc
