#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "parser.h"

namespace Cbc {

opcode_t bits(opcode opc) { return static_cast<opcode_t>(opc); }

using W = Width::Value;
using S = Sign::Value;
using IR = IReg::Value;
using FR = FReg::Value;
static constexpr W W8 = W::W8;
static constexpr W W16 = W::W16;
static constexpr W W32 = W::W32;
static constexpr W W64 = W::W64;
static constexpr S SIGN = S::SIGNED;
static constexpr S USIGN = S::UNSIGNED;
static constexpr IR IR1 = IR::IR1;

auto read_rr(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, IReg> res = Decoder::ByteReaderM(codeReader)
        .read4<IReg::Value>()
        .read4<IReg::Value>()
        .get();
    return res;
}

auto read_ri(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, uint8_t> res = Decoder::ByteReaderM(codeReader)
        .read4<IReg::Value>()
        .read4()
        .get();
    return res;
}

auto read_zr(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg> res = Decoder::ByteReaderM(codeReader)
        .read4()
        .read4<IReg::Value>()
        .get();
    return res;
}

auto read_xrrr(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, IReg> res =  Decoder::ByteReaderM(codeReader)
        .read4()
        .read4<IReg::Value>()
        .read4<IReg::Value>()
        .read4<IReg::Value>()
        .get();
    return res;
}

auto read_xfff(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, FReg> res =  Decoder::ByteReaderM(codeReader)
        .read4()
        .read4<FReg::Value>()
        .read4<FReg::Value>()
        .read4<FReg::Value>()
        .get();
    return res;
}

auto read_xrri(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, uint8_t> res =  Decoder::ByteReaderM(codeReader)
        .read4()
        .read4<IReg::Value>()
        .read4<IReg::Value>()
        .read4()
        .get();
    return res;
}

auto read_xffi(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, uint8_t> res =  Decoder::ByteReaderM(codeReader)
        .read4()
        .read4<FReg::Value>()
        .read4<FReg::Value>()
        .read4()
        .get();
    return res;
}

auto read_imm16(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return Decoder::ByteReaderM(codeReader)
        .read16()
        .get();
}

// using namespace opcodes;

// constexpr opcode_t local_opcode(opcode opc, opcode base, size_t module) { return ((opcode_t) opc - (opcode_t) base) % module; }
// constexpr common_opc common(opcode opc, opcode base) { return common_opc(local_opcode(opc, base, common_opc_sz)); }
// constexpr checked_opc checked(opcode opc, opcode base) { return checked_opc(local_opcode(opc, base, checked_opc_sz)); }

constexpr CbcTypeKind common_type(Width w, Sign s)
{
    switch(s) {
        case Sign::SIGNED: switch (w) {
            case Width::W8: return CbcTypeKind::I8;
            case Width::W16: return CbcTypeKind::I16;
            case Width::W32: return CbcTypeKind::I32;
            case Width::W64: return CbcTypeKind::I64;
        };
        case Sign::UNSIGNED: switch (w) {
            case Width::W8: return CbcTypeKind::U8;
            case Width::W16: return CbcTypeKind::U16;
            case Width::W32: return CbcTypeKind::U32;
            case Width::W64: return CbcTypeKind::U64;
        };
    default: ASSERTION(false, "unknown Sign");
    }
}

constexpr Sign checked_sign(uint8_t x)
{
    return Sign::Value(x & 0b1);
}

constexpr Width checked_width(uint8_t x)
{
    return Width::Value((x >> 1) & 0b11);
}

constexpr CbcTypeKind checked_type(uint8_t x)
{
    switch (x & 0b1) {
    case SIGN: {
        switch ((x >> 1) & 0b11) {
        case W8: { return CbcTypeKind::Value::I8; }
        case W16: { return CbcTypeKind::Value::I16; }
        case W32: { return CbcTypeKind::Value::I32; }
        case W64: { return CbcTypeKind::Value::I64; }
        };
    }
    case USIGN: {
        switch ((x >> 1) & 0b11) {
        case W8: { return CbcTypeKind::Value::U8; }
        case W16: { return CbcTypeKind::Value::U16; }
        case W32: { return CbcTypeKind::Value::U32; }
        case W64: { return CbcTypeKind::Value::U64; }
        };
    }
    };
}

constexpr Width float_width(uint8_t x)
{
    return Width::Value(((x >> 3) & 0b1) + W32); // opcCommon
}

constexpr CbcTypeKind float_type(uint8_t x)
{
    switch (float_width(x)) {
        case W32: { return CbcTypeKind::Value::F32; }
        case W64: { return CbcTypeKind::Value::F64; }
        default: ASSERTION(false, "unknown encoding");
    }
}

#include "isa_opcode_def.h"
GEN_OPCODE_DECODER_MOV_EXTEND(GEN_B2_MANUAL)
GEN_OPCODE_DECODER_COMMON(GEN_B2_COMMON)
GEN_OPCODE_DECODER_NEG(GEN_B2_MANUAL)
GEN_OPCODE_DECODER_INTEGER_COMMON(GEN_B3_COMMON)
GEN_OPCODE_DECODER_CHECKED(GEN_B3_CHECKED)
GEN_OPCODE_DECODER_BFX(EMPTY_IMPL)
GEN_OPCODE_DECODER_FLOAT_COMMON(GEN_B3_FLOAT)
GEN_OPCODE_DECODER_FLOAT_MISC(EMPTY_IMPL)
GEN_OPCODE_DECODER_SETIF(EMPTY_IMPL)
GEN_OPCODE_DECODER_RET(GEN_RET)
#include "isa_opcode_undef.h"

// template<>
// void Parser::decode<opcode::Mov32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoMov(d, r, false);
// }
//
// template<>
// void Parser::decode<opcode::Mov64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoMov(d, r, false);
// }
//
// template<>
// void Parser::decode<opcode::Mov32i, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoMovImm(W32, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Mov64i, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoMovImm(W64, d, r);
// }
//
// template<>
// void Parser::decode<opcode::MovVst, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoMovVST(d, r);
// }
//
// template<>
// void Parser::decode<opcode::MovRef, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoMov(d, r, true);
// }
//
// template<>
// void Parser::decode<opcode::ExtendSigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoExtend(SIGN, d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::ExtendUnsigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoExtend(USIGN, d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Add32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Sub32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Sub32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Mul32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Mul32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::And32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::And32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Or32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Or32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Xor32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Xor32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div32Signed, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Div32Signed, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem32Signed, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Rem32Signed, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div32Unsigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Div32Unsigned, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem32Unsigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Rem32Unsigned, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsr32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Lsr32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Asr32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Asr32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsl32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Lsl32, opcode::ExtendUnsigned), common(W32, SIGN), d, d, r);
// }

// template<>
// void Parser::decode<opcode::Neg32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoINeg(common(W32, SIGN), d, r);
// }

// template<>
// void Parser::decode<opcode::Add64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Sub64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Mul64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::And64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Or64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Xor64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div64Signed, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem64Signed, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div64Unsigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem64Unsigned, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsr64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Asr64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsl64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg32), common(W64, SIGN), d, d, r);
// }

// template<>
// void Parser::decode<opcode::Neg64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoINeg(common(W64, SIGN), d, r);
// }

// template<>
// void Parser::decode<opcode::Add32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Add64, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
//
// template<>
// void Parser::decode<opcode::Sub32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Sub32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Mul32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Mul32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::And32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::And32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Or32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Or32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Xor32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Xor32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div32SignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Div32SignedImm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem32SignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Rem32SignedImm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div32UnsignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Div32UnsignedImm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem32UnsignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Rem32UnsignedImm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsr32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Lsr32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Asr32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Asr32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsl32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Lsl32Imm, opcode::Neg64), common(W32, SIGN), d, d, r);
// }

// template<>
// void Parser::decode<opcode::Neg32Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoINeg(common(W32, SIGN), d, r);
// }

// template<>
// void Parser::decode<opcode::Add64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Add64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Sub64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Sub64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Mul64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Mul64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::And64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::And64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Or64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Or64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Xor64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Xor64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div64SignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Div64SignedImm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem64SignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Rem64SignedImm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Div64UnsignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Div64UnsignedImm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Rem64UnsignedImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Rem64UnsignedImm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsr64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Lsr64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Asr64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Asr64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }
//
// template<>
// void Parser::decode<opcode::Lsl64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_ri(codeReader);
//     DoCommonOp(common(opcode::Lsl64Imm, opcode::Neg32Imm), common(W64, SIGN), d, d, r);
// }

// template<>
// void Parser::decode<opcode::Neg64Imm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
//     DoINeg(common(W64, SIGN), d, r);
// }

// template<>
// void Parser::decode<opcode::IntegerCommon32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCommonOp(Common::Value(x), common(W32, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCommonOp(Common::Value(x), common(W64, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon32K0, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W32, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon64K0, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W64, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon32K8, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W32, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon64K8, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W64, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon32K16, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W32, SIGN), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::IntegerCommon64K16, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCommonOp(Common::Value(x), common(W64, SIGN), d, l, r);
// }

// template<>
// void Parser::decode<opcode::CheckedAdd, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCheckedOp(checked_opc::Add, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedSub, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCheckedOp(checked_opc::Sub, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedMul, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCheckedOp(checked_opc::Mul, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedDiv, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     DoCheckedOp(checked_opc::Div, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedAddImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCheckedOp(checked_opc::Add, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedSubImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCheckedOp(checked_opc::Sub, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedMulImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCheckedOp(checked_opc::Mul, checked(x), d, l, r);
// }
//
// template<>
// void Parser::decode<opcode::CheckedDivImm, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     DoCheckedOp(checked_opc::Div, checked(x), d, l, r);
// }

// template<>
// void Parser::decode<opcode::Bfx, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrri(codeReader);
//     // DoBFX(x & 0b1, (x >> 1) & 0x1, (x >> 2) & 0b1, d, l, r);
// }

// template<>
// void Parser::decode<opcode::FloatCommon, void>(Decoder::ByteReader& codeReader)
// {
//     auto [x, d, l, r] = read_xrrr(codeReader);
//     // DoBinaryFloatOp(float_opc(x), CbcTypeKind tkind, FReg dst, FReg src1, FReg src2)
// }

// template<>
// void Parser::decode<opcode::FloatMisc, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
// }
//
// template<>
// void Parser::decode<opcode::SetIf32, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
// }
//
// template<>
// void Parser::decode<opcode::SetIf64, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
// }
//
// template<>
// void Parser::decode<opcode::SetIf32Float, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
// }
//
// template<>
// void Parser::decode<opcode::SetIf64Float, void>(Decoder::ByteReader& codeReader)
// {
//     auto [d, r] = read_rr(codeReader);
// }

} // namespace Cbc
