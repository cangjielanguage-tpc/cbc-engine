#include "cbc/parser.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"

namespace Cbc {

void Parser::Interpret() {
    while (!codeReader.EndOfMem(codeEnd)) {
        auto opcode = codeReader.PeekOpcode();
        InterpretOne(opcode);
    }
}

void Parser::InterpretOne(uint32_t opcode) {
    switch (opcode) {
        case B2rr::Fmt(Common::MOV, Width::W32):  B2rrMov(B2rr::Decode(codeReader), Width::W32, false); break;
        case B2rr::Fmt(Common::MOV, Width::W64):  B2rrMov(B2rr::Decode(codeReader), Width::W64, false); break;
        case B2rr::Fmt(Common::MVST, Width::W32): B2rrMovVST(B2rr::Decode(codeReader)); break;
        case B2rr::Fmt(Common::MREF, Width::W64): B2rrMov(B2rr::Decode(codeReader), Width::W64, true); break;
        
        case B2rr::Fmt(Common::ADD, Width::W32):  B2rrCommon(B2rr::Decode(codeReader), Common::ADD, CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::ADD, Width::W64):  B2rrCommon(B2rr::Decode(codeReader), Common::ADD, CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::SUB, Width::W32):  B2rrSub(B2rr::Decode(codeReader), CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::SUB, Width::W64):  B2rrSub(B2rr::Decode(codeReader), CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::MUL, Width::W32):  B2rrCommon(B2rr::Decode(codeReader), Common::MUL, CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::MUL, Width::W64):  B2rrCommon(B2rr::Decode(codeReader), Common::MUL, CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::AND, Width::W32):  B2rrCommon(B2rr::Decode(codeReader), Common::AND, CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::AND, Width::W64):  B2rrCommon(B2rr::Decode(codeReader), Common::AND, CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::OR,  Width::W32):  B2rrCommon(B2rr::Decode(codeReader), Common::OR,  CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::OR,  Width::W64):  B2rrCommon(B2rr::Decode(codeReader), Common::OR,  CbcTypeKind::I64); break;
        case B2rr::Fmt(Common::XOR, Width::W32):  B2rrCommon(B2rr::Decode(codeReader), Common::XOR, CbcTypeKind::I32); break;
        case B2rr::Fmt(Common::XOR, Width::W64):  B2rrCommon(B2rr::Decode(codeReader), Common::XOR, CbcTypeKind::I64); break;

        case B2hr::Fmt(Common::MOV, Width::W32):  B2hrMov(B2hr::Decode(codeReader), Width::W32); break;
        case B2hr::Fmt(Common::MOV, Width::W64):  B2hrMov(B2hr::Decode(codeReader), Width::W64); break;
        case B2hr::Fmt(Common::ADD, Width::W32):  B2hrCommon(B2hr::Decode(codeReader), Common::ADD, CbcTypeKind::I32); break;
        case B2hr::Fmt(Common::ADD, Width::W64):  B2hrCommon(B2hr::Decode(codeReader), Common::ADD, CbcTypeKind::I64); break;
        case B2hr::Fmt(Common::MUL, Width::W32):  B2hrCommon(B2hr::Decode(codeReader), Common::MUL, CbcTypeKind::I32); break;
        case B2hr::Fmt(Common::MUL, Width::W64):  B2hrCommon(B2hr::Decode(codeReader), Common::MUL, CbcTypeKind::I64); break;
        case B2hr::Fmt(Common::AND, Width::W32):  B2hrCommon(B2hr::Decode(codeReader), Common::AND, CbcTypeKind::I32); break;
        case B2hr::Fmt(Common::AND, Width::W64):  B2hrCommon(B2hr::Decode(codeReader), Common::AND, CbcTypeKind::I64); break;
        case B2hr::Fmt(Common::OR,  Width::W32):  B2hrCommon(B2hr::Decode(codeReader), Common::OR,  CbcTypeKind::I32); break;
        case B2hr::Fmt(Common::OR,  Width::W64):  B2hrCommon(B2hr::Decode(codeReader), Common::OR,  CbcTypeKind::I64); break;
        case B2hr::Fmt(Common::XOR, Width::W32):  B2hrCommon(B2hr::Decode(codeReader), Common::XOR, CbcTypeKind::I32); break;
        case B2hr::Fmt(Common::XOR, Width::W64):  B2hrCommon(B2hr::Decode(codeReader), Common::XOR, CbcTypeKind::I64); break;
        case B2hr::Fmt(Common::EXT, Sign::SIGNED):   B2hrExtend(B2hr::Decode(codeReader), Sign::SIGNED); break;
        case B2hr::Fmt(Common::EXT, Sign::UNSIGNED): B2hrExtend(B2hr::Decode(codeReader), Sign::UNSIGNED); break;

        case B3xrrr::Fmt(OP7A::COMMON, Sign::SIGNED): B3xrrrCommon(B3xrrr::Decode(codeReader), Sign::SIGNED); break;

        case SymbolicObjectControl::Opc0100::OPCODE: B2xrOpc0100SOC(SymbolicObjectControl::B2xr::Decode(codeReader)); break;

        default:
            ASSERTION(false, "Unexpected opcode");
            break;
    }
}

void Parser::B2rrMov(B2rr args, Width width, bool isReference) {
    DoMov(width, args.rr.x.IR(), args.rr.y.IR(), isReference);
}

void Parser::B2rrMovVST(B2rr args) {
    DoMovVST(args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2rrCommon(B2rr args, Common op, CbcTypeKind tkind) {
    DoCommonOp(op, tkind, args.rr.x.IR(), args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2rrSub(B2rr args, CbcTypeKind tkind) {
    auto rx = args.rr.x.IR(), ry = args.rr.y.IR();
    if (rx == IReg::IRZ) {
        DoINeg(tkind, ry, ry);
    } else {
        DoCommonOp(Common::SUB, tkind, rx, rx, ry);
    }
}

void Parser::B2hrMov(B2hr args, Width width) {
    DoMovImm(width, args.xr.r.IR(), args.xr.imm); // TODO: decode imm
}

void Parser::B2hrCommon(B2hr args, Common op, CbcTypeKind tkind) {
    auto dst = args.xr.r.IR();
    Width::Value width;
    if (tkind == CbcTypeKind::I32) {
        width = Width::W32;
    } else {
        assert(tkind == CbcTypeKind::I64);
        width = Width::W64;
    }
    DoCommonOp(op, tkind, dst, dst, args.xr.imm); // TODO: decode imm
}

void Parser::B2hrExtend(B2hr args, Sign sign) {
    DoExtend(sign, args.xr.r.IR(), args.xr.r.IR(), args.xr.imm); // TODO: decode imm
}

void Parser::B3xrrrCommon(B3xrrr args, Sign sign) {
    auto op = Common(sign.ToBits().Shift(3) | (args.xr.imm >> 1));
    auto tkind = (args.xr.imm & 0b1) ? CbcTypeKind::I64 : CbcTypeKind::I32;
    DoCommonOp(op, tkind, args.rr.x.IR(), args.rr.x.IR(), args.rr.y.IR());
}

void Parser::B2xrOpc0100SOC(SymbolicObjectControl::B2xr args) {
    switch (SymbolicObjectControl::Opc0100(args.xr.imm)) {
        case SymbolicObjectControl::Opc0100::RET_32:  DoReturn(Width::W32, args.xr.r.IR()); break;
        case SymbolicObjectControl::Opc0100::RET_64:  DoReturn(Width::W64, args.xr.r.IR()); break;
        case SymbolicObjectControl::Opc0100::FRET_32: DoReturn(Width::W32, args.xr.r.FR()); break;
        case SymbolicObjectControl::Opc0100::FRET_64: DoReturn(Width::W64, args.xr.r.FR()); break;
        
        default: ASSERTION(false, "Not implemented"); break;
    }
}

} // namespace Cbc