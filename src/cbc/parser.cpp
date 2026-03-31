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
    return std::move(x);
}

auto ReadRR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<IReg, IReg> res = Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4<IReg::Value>().Get();
    return res;
}

auto ReadRZI16(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<IReg, uint8_t, uint32_t> res =
        Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Read16().Get();
    return res;
}

auto ReadRZI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<IReg, uint8_t, uint32_t> res =
        Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Read32().Get();
    return res;
}

auto ReadRZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<IReg, uint8_t, uint64_t> res =
        Decoder::ByteReaderM(codeReader).Read4<IReg::Value>().Read4().Read64().Get();
    return res;
}

auto ReadZR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, IReg> res = Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Get();
    return res;
}

auto ReadXRRR(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, IReg, IReg, IReg> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4<IReg::Value>().Get();
    return res;
}

auto ReadXFFF(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, FReg, FReg, FReg> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4<FReg::Value>().Get();
    return res;
}

auto ReadXRRZ(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, IReg, IReg, uint8_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4().Get();
    return res;
}

auto ReadXRRI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, IReg, IReg, uint8_t, uint32_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4().Read32().Get();
    return res;
}

auto ReadXRRZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, IReg, IReg, uint8_t, uint64_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<IReg::Value>().Read4<IReg::Value>().Read4().Read64().Get();
    return res;
}

auto ReadXFFZ(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, FReg, FReg, uint8_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4().Get();
    return res;
}

auto ReadXFFZI32(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, FReg, FReg, uint8_t, uint32_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4().Read32().Get();
    return res;
}

auto ReadXFFZI64(Decoder::ByteReader& codeReader) -> decltype(auto)
{
    std::tuple<uint8_t, FReg, FReg, uint8_t, uint64_t> res =
        Decoder::ByteReaderM(codeReader).Read4().Read4<FReg::Value>().Read4<FReg::Value>().Read4().Read64().Get();
    return res;
}

auto ReadImm8(Decoder::ByteReader& codeReader) -> decltype(auto) { return codeReader.Read8(); }

auto ReadImm16(Decoder::ByteReader& codeReader) -> decltype(auto) { return codeReader.Read16(); }

auto ReadImm32(Decoder::ByteReader& codeReader) -> decltype(auto) { return codeReader.Read32(); }

auto ReadImm64(Decoder::ByteReader& codeReader) -> decltype(auto) { return codeReader.Read64(); }

constexpr Sign GetCheckedSign(uint8_t x) { return Sign::Value(x & 0b1); }

constexpr Width GetCheckedWidth(uint8_t x) { return Width::Value((x >> 1) & 0b11); }

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

template <> void Parser::Decode<InputOpcode::Mov32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r, false);
}

template <> void Parser::Decode<InputOpcode::Mov64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r, false);
}

template <> void Parser::Decode<InputOpcode::Mov32i>()
{
    auto [d, z, imm32] = ReadRZI32(codeReader);
    DoMovImm(W32, d, imm32);
}

template <> void Parser::Decode<InputOpcode::Mov64i>()
{
    auto [d, z, imm64] = ReadRZI64(codeReader);
    DoMovImm(W64, d, imm64);
}

template <> void Parser::Decode<InputOpcode::MovRef>()
{
    auto [d, r] = ReadRR(codeReader);
    DoMov(d, r, true);
}

template <> void Parser::Decode<InputOpcode::Add32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Add, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Sub32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Sub, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Mul32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Mul, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::And32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::And, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Or32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Or, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Xor32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Xor, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::UDiv32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::DivUnsigned, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::URem32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::RemUnsigned, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::LSR32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Lsr, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::ASR32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Asr, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::LSL32>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Lsl, W32, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Add64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Add, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Sub64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Sub, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Mul64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Mul, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::And64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::And, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Or64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Or, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Xor64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Xor, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::UDiv64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::DivUnsigned, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::URem64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::RemUnsigned, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::LSR64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Lsr, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::ASR64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Asr, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::LSL64>()
{
    auto [d, r] = ReadRR(codeReader);
    DoBinary(InputCommonOpc::Lsl, W64, d, d, r);
}

template <> void Parser::Decode<InputOpcode::Binary32>()
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoBinary(InputCommonOpc(x), W32, d, l, r);
}

template <> void Parser::Decode<InputOpcode::Binary64>()
{
    auto [x, d, l, r] = ReadXRRR(codeReader);
    DoBinary(InputCommonOpc(x), W64, d, l, r);
}

template <> void Parser::Decode<InputOpcode::BinaryImm32>()
{
    auto [x, d, l, z, imm32] = ReadXRRI32(codeReader);
    DoBinaryImm(InputCommonOpc(x), W32, d, l, imm32);
}

template <> void Parser::Decode<InputOpcode::BinaryImm64>()
{
    auto [x, d, l, z, imm64] = ReadXRRZI64(codeReader);
    DoBinaryImm(InputCommonOpc(x), W64, d, l, imm64);
}

template <> void Parser::Decode<InputOpcode::Bcc>()
{
    auto [cc, w, l, r, target] = Decoder::ByteReaderM(codeReader)
                                     .Read4<InputCcOpc>()
                                     .Read4<Width::Value>()
                                     .Read4<IReg::Value>()
                                     .Read4<IReg::Value>()
                                     .Read32<int32_t>()
                                     .Get();
    DoBranchIf(cc, w, l, r, codeReader.Cursor() + target);
}

template <> void Parser::Decode<InputOpcode::BccImm>()
{
    auto [cc, w, z, l, imm64, target] = Decoder::ByteReaderM(codeReader)
                                            .Read4<InputCcOpc>()
                                            .Read4<Width::Value>()
                                            .Read4()
                                            .Read4<IReg::Value>()
                                            .Read64<uint64_t>()
                                            .Read32<int32_t>()
                                            .Get();
    DoBranchIfImm(cc, w, l, imm64, codeReader.Cursor() + target);
}

template <> void Parser::Decode<InputOpcode::Jump32>()
{
    auto [target] = Decoder::ByteReaderM(codeReader).Read32<int32_t>().Get();
    DoJmp(codeReader.Cursor() + target);
}

template <> void Parser::Decode<InputOpcode::CallDirect>()
{
    auto [d, z, id16] = ReadRZI16(codeReader);
    DoCallDirect(d, id16);
}

template <> void Parser::Decode<InputOpcode::Ret32>()
{
    auto [d, r] = ReadZR(codeReader);
    DoReturn(W32, r);
}

template <> void Parser::Decode<InputOpcode::Ret64>()
{
    auto [d, r] = ReadZR(codeReader);
    DoReturn(W64, r);
}

template <> void Parser::Decode<InputOpcode::Ret32F>()
{
    auto [d, r] = ReadZR(codeReader);
    DoReturn(W32, r);
}

template <> void Parser::Decode<InputOpcode::Ret64F>()
{
    auto [d, r] = ReadZR(codeReader);
    DoReturn(W64, r);
}

static void UnexpectedOpcode(uint32_t opcode)
{
    // FIXME: verbose error reporting.
    ASSERTION(false, "unexpected opcode");
}

template <InputOpcode opcode> void Parser::Decode() { UnexpectedOpcode(static_cast<uint32_t>(opcode)); }

struct ParserTableGenerator {
    using DecodeOp = void (Parser::*)();

    template <std::size_t... opcodes> static constexpr auto Gen(std::index_sequence<opcodes...>)
    {
        return std::array<DecodeOp, sizeof...(opcodes)> { &Parser::Decode<static_cast<InputOpcode>(opcodes)>... };
    }

    static constexpr auto opcodes = std::make_index_sequence<static_cast<std::size_t>(InputOpcode::___LAST)> {};
};

static constexpr auto table = ParserTableGenerator::Gen(ParserTableGenerator::opcodes);

inline void Cbc::Parser::InterpretOne(uint32_t opcode)
{
    if (opcode < static_cast<uint32_t>(InputOpcode::___LAST)) {
        (this->*table[opcode])();
    } else {
        UnexpectedOpcode(opcode);
    }
}

} // namespace Cbc
