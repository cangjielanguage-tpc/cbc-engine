#include "cbc/isa.h"
#include "cbc/decoder.h"
#include "cbc/parser.h"
#include <tuple>
#include <type_traits>

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

auto ReadRI(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<IReg, uint8_t> res = Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Get();
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

auto ReadXRRI(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, IReg, IReg, uint8_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4().Get();
    return res;
}

auto ReadXFFI(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    ::std::tuple<uint8_t, FReg, FReg, uint8_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4().Get();
    return res;
}

auto ReadImm32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return ::std::move(Decoder::ByteReaderM(codeReader).Read32<int64_t>().Get());
}

auto ReadImm64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    return ::std::move(Decoder::ByteReaderM(codeReader).Read64<int64_t>().Get());
}

uint64_t ReadImmPrefix(Decoder::ByteReader& codeReader, ImmPrefix immPrefix)
{
    if (immPrefix == ImmPrefix::ImmPrefix32) {
        auto [x] = ReadImm32(codeReader);
        immPrefix = ImmPrefix::__LAST;
        return ::std::move(x);
    } else if (immPrefix == ImmPrefix::ImmPrefix64) { /* ImmPrefix64 */
        auto [x] = ReadImm64(codeReader);
        immPrefix = ImmPrefix::__LAST;
        return ::std::move(x);
    } else {
        return 0;
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

// #include "isa_opcode_def.h"
// GEN_OPCODE_DECODER_MOV_EXTEND(GEN_B2_MANUAL)
// GEN_OPCODE_DECODER_COMMON(GEN_B2_COMMON)
// GEN_OPCODE_DECODER_NEG(GEN_B2_MANUAL)
// GEN_OPCODE_DECODER_INTEGER_COMMON(GEN_B3_COMMON)
// GEN_OPCODE_DECODER_CHECKED(GEN_B3_CHECKED)
// GEN_OPCODE_DECODER_BFX(EMPTY_IMPL)
// GEN_OPCODE_DECODER_FLOAT_COMMON(GEN_B3_FLOAT)
// GEN_OPCODE_DECODER_FLOAT_MISC(EMPTY_IMPL)
// GEN_OPCODE_DECODER_SETIF(EMPTY_IMPL)
// GEN_OPCODE_DECODER_RET(GEN_RET)
// #include "isa_opcode_undef.h"

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader);
    DoMov( d, r , false);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader);
    DoMov( d, r , false);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov32i>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoMovImm(W32, d, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mov64i>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoMovImm(W64, d, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovVst>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader);
    DoMovVST( d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::MovRef>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader);
    DoMov( d, r , true);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendSigned>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoExtend(SIGN, d, d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::ExtendUnsigned>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoExtend(USIGN, d, d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Add, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Sub, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Mul, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::And, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Or, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Xor, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::DivSigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::RemSigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::DivUnsigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::RemUnsigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsr, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Asr, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsl, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Add, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Sub, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Mul, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::And, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Or, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Xor, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::DivSigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::RemSigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::DivUnsigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::RemUnsigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsr, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Asr, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsl, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Add, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Sub, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Mul, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::And32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::And, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Or, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Xor, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::DivSigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::RemSigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::DivUnsigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::RemUnsigned, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsr, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Asr, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsl, ::Cbc::GetCommonType(W32, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Add64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Add, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Sub64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Sub, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Mul64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Mul, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::And64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::And, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Or64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Or, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Xor64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Xor, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivSigned64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::DivSigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemSigned64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::RemSigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::DivUnsigned64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::DivUnsigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::RemUnsigned64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::RemUnsigned, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsr64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsr, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Asr64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Asr, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Lsl64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoCommonOp(::Cbc::CommonOpc::Lsl, ::Cbc::GetCommonType(W64, SIGN), d, d, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoINeg(::Cbc::GetCommonType(W32, SIGN), d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRR(codeReader); DoINeg(::Cbc::GetCommonType(W64, SIGN), d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg32Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoINeg(::Cbc::GetCommonType(W32, SIGN), d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Neg64Imm>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadRI(codeReader); DoINeg(::Cbc::GetCommonType(W64, SIGN), d, r );
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCommonOp(::Cbc::CommonOpc(x), ::Cbc::GetCommonType(W32, SIGN), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCommonOp(::Cbc::CommonOpc(x), ::Cbc::GetCommonType(W64, SIGN), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon32K16>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCommonOp(::Cbc::CommonOpc(x), ::Cbc::GetCommonType(W32, SIGN), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::IntegerCommon64K16>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCommonOp(::Cbc::CommonOpc(x), ::Cbc::GetCommonType(W64, SIGN), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAdd>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCheckedOp(::Cbc::CheckedOpc::Add, ::Cbc::GetCheckedType(x), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSub>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCheckedOp(::Cbc::CheckedOpc::Sub, ::Cbc::GetCheckedType(x), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMul>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCheckedOp(::Cbc::CheckedOpc::Mul, ::Cbc::GetCheckedType(x), d, l, r);
}


template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDiv>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRR(codeReader);
    DoCheckedOp(::Cbc::CheckedOpc::Div, ::Cbc::GetCheckedType(x), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedAddImm>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCheckedOp(::Cbc::CheckedOpc::Add, ::Cbc::GetCheckedType(x), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedSubImm>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCheckedOp(::Cbc::CheckedOpc::Sub, ::Cbc::GetCheckedType(x), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedMulImm>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCheckedOp(::Cbc::CheckedOpc::Mul, ::Cbc::GetCheckedType(x), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::CheckedDivImm>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXRRI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoCheckedOp(::Cbc::CheckedOpc::Div, ::Cbc::GetCheckedType(x), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Bfx>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommon>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXFFF(codeReader);
    DoBinaryFloatOp(::Cbc::FloatOpc(x), ::Cbc::GetFloatType(x), d, l, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatCommonImm>(::Decoder::ByteReader & codeReader)
{
    auto [x, d, l, r] = ::Cbc::ReadXFFI(codeReader);
    uint64_t imm = ReadImmPrefix(codeReader, immPrefix);
    DoBinaryFloatOp(::Cbc::FloatOpc(x), ::Cbc::GetFloatType(x), d, l, imm);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatIntegerConversions>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::FloatFloatConversions>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf32>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf64>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf32Float>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::SetIf64Float>(::Decoder::ByteReader & codeReader)
{

}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadZR(codeReader); DoReturn(W32, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadZR(codeReader); DoReturn(W64, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret32F>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadZR(codeReader); DoReturn(W32, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::Ret64F>(::Decoder::ByteReader & codeReader)
{
    auto [d, r] = ::Cbc::ReadZR(codeReader); DoReturn(W64, r);
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix32>(::Decoder::ByteReader & codeReader)
{
    immPrefix = ImmPrefix::ImmPrefix32;
}

template <> void ::Cbc::Parser::Decode<::Cbc::InputOpcode::ImmPrefix64>(::Decoder::ByteReader & codeReader)
{
    immPrefix = ImmPrefix::ImmPrefix64;
}

} // namespace Cbc
