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
        InterpretOne(codeReader.Read8());
    }
}

#include "isa_opcode_def.h"
GEN_JUMP_TABLE(DO_WITH_ALL_OPCODES(GEN_JUMP_TABLE_ENTRY))
#include "isa_opcode_undef.h"

void Parser::InterpretImmExt(uint64_t imm, uint32_t bits, Sign sign) { immDecoder.SetImmExt(imm, bits, sign); }

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

} // namespace Cbc
