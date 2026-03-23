#include "cbc/decoder.h"
#include "cbc/isa.h"

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

} // namespace Cbc
