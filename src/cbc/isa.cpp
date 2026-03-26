#include "cbc/isa.h"
#include "cbc/decoder.h"
#include "cbc/parser.h"
#include <bits/types/cookie_io_functions_t.h>
#include <tuple>

namespace Cbc {

using W                  = Width::Value;
using S                  = Sign::Value;
using IR                 = IReg::Value;
using FR                 = FReg::Value;
static constexpr W W8    = W::W8;
static constexpr W W16   = W::W16;
static constexpr W W32   = W::W32;
static constexpr W W64   = W::W64;
static constexpr S SIGN  = S::SIGNED;
static constexpr S USIGN = S::UNSIGNED;
static constexpr IR IR1  = IR::IR1;

auto ReadOp(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    auto [x] = Decoder::ByteReaderM(codeReader).Read8().Get();
    return ::std::move(x);
}

auto ReadRR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, IReg> res = Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4<IReg::Value>().Get();
    return res;
}

auto ReadRZI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, uint8_t, uint32_t> res = Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Read32().Get();
    return res;
}

auto ReadRZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, uint8_t, uint64_t> res = Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Read64().Get();
    return res;
}

auto ReadZR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg> res = Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Get();
    return res;
}

auto ReadXRRR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, IReg> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4<IReg::Value>().Get();
    return res;
}

auto ReadXFFF(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, FReg> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4<FReg::Value>().Get();
    return res;
}

auto ReadXRRZ(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, uint8_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<IReg::Value>()
        .Read4<IReg::Value>()
        .Read4()
        .Get();
    return res;
}

auto ReadXRRI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, uint8_t, uint32_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<IReg::Value>()
        .Read4<IReg::Value>()
        .Read4()
        .Read32()
        .Get();
    return res;
}

auto ReadXRRZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, uint8_t, uint64_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<IReg::Value>()
        .Read4<IReg::Value>()
        .Read4()
        .Read64()
        .Get();
    return res;
}

auto ReadXFFZ(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, uint8_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<FReg::Value>()
        .Read4<FReg::Value>()
        .Read4()
        .Get();
    return res;
}

auto ReadXFFZI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, uint8_t, uint32_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<FReg::Value>()
        .Read4<FReg::Value>()
        .Read4()
        .Read32()
        .Get();
    return res;
}

auto ReadXFFZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, uint8_t, uint64_t> res = Decoder::ByteReaderM(codeReader)
        .Read4()
        .Read4<FReg::Value>()
        .Read4<FReg::Value>()
        .Read4()
        .Read64()
        .Get();
    return res;
}

auto ReadImm8(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return codeReader.Read8();
}

auto ReadImm16(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return codeReader.Read16();
}

auto ReadImm32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return codeReader.Read32();
}

auto ReadImm64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return codeReader.Read64();
}

uint64_t ReadImmPrefix(Decoder::ByteReader& codeReader, ImmPrefix immPrefix)
{
    switch (immPrefix) {
    case ImmPrefix::ImmPrefix32: {
        immPrefix = ImmPrefix::__LAST;
        return ReadImm32(codeReader);
    }
    case ImmPrefix::ImmPrefix64: {
        immPrefix = ImmPrefix::__LAST;
        return ReadImm64(codeReader);
    }
    default: ASSERTION(false, "immPrefix is not set");
    }
}

uint64_t ReadImmPrefix(Decoder::ByteReader& codeReader, Width immWidth)
{
    switch (immWidth) {
    case W8: { return ReadImm8(codeReader); }
    case W16: { return ReadImm16(codeReader); }
    case W32: { return ReadImm32(codeReader); }
    case W64: { return ReadImm64(codeReader); }
    default: ASSERTION(false, "unknown Width");
    }
}

constexpr CbcTypeKind GetCommonType(Width w, Sign s)
{
    switch (s) {
        case Sign::SIGNED:
            switch (w) {
                case Width::W8:  return CbcTypeKind::I8;
                case Width::W16: return CbcTypeKind::I16;
                case Width::W32: return CbcTypeKind::I32;
                case Width::W64: return CbcTypeKind::I64;
            };
        case Sign::UNSIGNED:
            switch (w) {
                case Width::W8:  return CbcTypeKind::U8;
                case Width::W16: return CbcTypeKind::U16;
                case Width::W32: return CbcTypeKind::U32;
                case Width::W64: return CbcTypeKind::U64;
            };
        default: ASSERTION(false, "unknown Sign");
    }
}

constexpr Sign GetCheckedSign(uint8_t x) { return Sign::Value(x & 0b1); }

constexpr Width GetCheckedWidth(uint8_t x) { return Width::Value((x >> 1) & 0b11); }

constexpr CbcTypeKind GetCheckedType(uint8_t x)
{
    switch (x & 0b1) {
        case SIGN: {
            switch ((x >> 1) & 0b11) {
                case W8: {
                    return CbcTypeKind::Value::I8;
                }
                case W16: {
                    return CbcTypeKind::Value::I16;
                }
                case W32: {
                    return CbcTypeKind::Value::I32;
                }
                case W64: {
                    return CbcTypeKind::Value::I64;
                }
            };
        }
        case USIGN: {
            switch ((x >> 1) & 0b11) {
                case W8: {
                    return CbcTypeKind::Value::U8;
                }
                case W16: {
                    return CbcTypeKind::Value::U16;
                }
                case W32: {
                    return CbcTypeKind::Value::U32;
                }
                case W64: {
                    return CbcTypeKind::Value::U64;
                }
            };
        }
        default: ASSERT(false && "unexpected format type");
    };
}

constexpr Width GetFloatWidth(uint8_t x)
{
    return Width::Value(((x >> 3) & 0b1) + W32); // opcCommon
}

constexpr CbcTypeKind GetFloatType(uint8_t x)
{
    switch (GetFloatWidth(x)) {
        case W32: {
            return CbcTypeKind::Value::F32;
        }
        case W64: {
            return CbcTypeKind::Value::F64;
        }
        default: ASSERTION(false, "unknown encoding");
    }
}

template <> void Parser::Decode<InputOpcode::Mov32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r , false);
}

template <> void Parser::Decode<InputOpcode::Mov64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r , false);
}

template <> void Parser::Decode<InputOpcode::Mov32i>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoMovImm(W32, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Mov64i>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoMovImm(W64, d, imm64);
}

template <> void Parser::Decode<InputOpcode::MovVst>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoMovVST(d, r);
}

template <> void Parser::Decode<InputOpcode::MovRef>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r , true);
}

template <> void Parser::Decode<InputOpcode::ExtendSigned>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoExtend(SIGN, d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::ExtendUnsigned>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoExtend(USIGN, d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Add32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Add, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Sub32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Sub, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Mul32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Mul, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::And32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::And, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Or32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Or, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Xor32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Xor, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::DivSigned32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::DivSigned, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::RemSigned32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::RemSigned, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::DivUnsigned32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::DivUnsigned, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::RemUnsigned32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::RemUnsigned, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Lsr32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Lsr, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Asr32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Asr, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Lsl32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Lsl, GetCommonType(W32, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Add64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Add, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Sub64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Sub, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Mul64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Mul, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::And64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::And, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Or64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Or, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Xor64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Xor, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::DivSigned64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::DivSigned, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::RemSigned64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::RemSigned, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::DivUnsigned64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::DivUnsigned, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::RemUnsigned64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::RemUnsigned, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Lsr64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Lsr, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Asr64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Asr, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Lsl64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoCommonOp(CommonOpc::Lsl, GetCommonType(W64, SIGN), d, d, r);
}

template <> void Parser::Decode<InputOpcode::Add32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Add, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Sub32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Sub, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Mul32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Mul, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::And32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::And, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Or32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Or, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Xor32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Xor, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::DivSigned32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::DivSigned, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::RemSigned32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::RemSigned, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::DivUnsigned32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::DivUnsigned, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::RemUnsigned32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::RemUnsigned, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Lsr32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Lsr, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Asr32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Asr, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Lsl32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoCommonOp(CommonOpc::Lsl, GetCommonType(W32, SIGN), d, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Add64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Add, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Sub64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Sub, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Mul64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Mul, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::And64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::And, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Or64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Or, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Xor64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Xor, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::DivSigned64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::DivSigned, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::RemSigned64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::RemSigned, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::DivUnsigned64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::DivUnsigned, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::RemUnsigned64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::RemUnsigned, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Lsr64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Lsr, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Asr64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Asr, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Lsl64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoCommonOp(CommonOpc::Lsl, GetCommonType(W64, SIGN), d, d, imm64);
}

template <> void Parser::Decode<InputOpcode::Neg32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoINeg(GetCommonType(W32, SIGN), d, r);
}

template <> void Parser::Decode<InputOpcode::Neg64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadRR(codeReader);
    DoINeg(GetCommonType(W64, SIGN), d, r);
}

template <> void Parser::Decode<InputOpcode::Neg32Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoINeg(GetCommonType(W32, SIGN), d, imm32);
}

template <> void Parser::Decode<InputOpcode::Neg64Imm>(::Decoder::ByteReader& codeReader)
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoINeg(GetCommonType(W64, SIGN), d, imm64);
}

template <> void Parser::Decode<InputOpcode::IntegerCommon32>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCommonOp(CommonOpc(x), GetCommonType(W32, SIGN), d, l, r);
}

template <> void Parser::Decode<InputOpcode::IntegerCommon64>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCommonOp(CommonOpc(x), GetCommonType(W64, SIGN), d, l, r);
}

template <> void Parser::Decode<InputOpcode::IntegerCommon32K16>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z, imm32] = ReadXRRI32(codeReader);
    DoCommonOp(CommonOpc(x), GetCommonType(W32, SIGN), d, l, imm32);
}

template <> void Parser::Decode<InputOpcode::IntegerCommon64K16>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z, imm64] = ReadXRRZI64(codeReader);
    DoCommonOp(CommonOpc(x), GetCommonType(W64, SIGN), d, l, imm64);
}

template <> void Parser::Decode<InputOpcode::CheckedAdd>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCheckedOp(CheckedOpc::Add, GetCheckedType(x), d, l, r);
}

template <> void Parser::Decode<InputOpcode::CheckedSub>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCheckedOp(CheckedOpc::Sub, GetCheckedType(x), d, l, r);
}

template <> void Parser::Decode<InputOpcode::CheckedMul>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCheckedOp(CheckedOpc::Mul, GetCheckedType(x), d, l, r);
}


template <> void Parser::Decode<InputOpcode::CheckedDiv>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoCheckedOp(CheckedOpc::Div, GetCheckedType(x), d, l, r);
}

template <> void Parser::Decode<InputOpcode::CheckedAddImm>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z] = ReadXRRZ(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, GetCheckedWidth(x));
    DoCheckedOp(CheckedOpc::Add, GetCheckedType(x), d, l, imm);
}

template <> void Parser::Decode<InputOpcode::CheckedSubImm>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z] = ReadXRRZ(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, GetCheckedWidth(x));
    DoCheckedOp(CheckedOpc::Sub, GetCheckedType(x), d, l, imm);
}

template <> void Parser::Decode<InputOpcode::CheckedMulImm>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z] = ReadXRRZ(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, GetCheckedWidth(x));
    DoCheckedOp(CheckedOpc::Mul, GetCheckedType(x), d, l, imm);
}

template <> void Parser::Decode<InputOpcode::CheckedDivImm>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z] = ReadXRRZ(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, GetCheckedWidth(x));
    DoCheckedOp(CheckedOpc::Div, GetCheckedType(x), d, l, imm);
}

template <> void Parser::Decode<InputOpcode::Bfx>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::FloatCommon>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, r] = ReadXFFF(codeReader);
    DoBinaryFloatOp(FloatOpc(x), GetFloatType(x), d, l, r);
}

template <> void Parser::Decode<InputOpcode::FloatCommonImm>(::Decoder::ByteReader& codeReader)
{
    auto [x, d, l, z] = ReadXFFZ(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoBinaryFloatOp(FloatOpc(x), GetFloatType(x), d, l, imm);
}

template <> void Parser::Decode<InputOpcode::FloatIntegerConversions>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::FloatFloatConversions>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::SetIf32>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::SetIf64>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::SetIf32Float>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::SetIf64Float>(::Decoder::ByteReader& codeReader)
{

}

template <> void Parser::Decode<InputOpcode::Ret32>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadZR(codeReader); DoReturn(W32, r);
}

template <> void Parser::Decode<InputOpcode::Ret64>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadZR(codeReader); DoReturn(W64, r);
}

template <> void Parser::Decode<InputOpcode::Ret32F>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadZR(codeReader); DoReturn(W32, r);
}

template <> void Parser::Decode<InputOpcode::Ret64F>(::Decoder::ByteReader& codeReader)
{
    auto [d, r] = ReadZR(codeReader); DoReturn(W64, r);
}

template <> void Parser::Decode<InputOpcode::ImmPrefix32>(::Decoder::ByteReader& codeReader)
{
    immPrefix = ImmPrefix::ImmPrefix32;
}

template <> void Parser::Decode<InputOpcode::ImmPrefix64>(::Decoder::ByteReader& codeReader)
{
    immPrefix = ImmPrefix::ImmPrefix64;
}

} // namespace Cbc
