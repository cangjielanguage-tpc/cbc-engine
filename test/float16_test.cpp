#include "cbc/c_abi.h"
#include "cbc/isa_disasm.h"
#include "cbc/platforms.h"
#include "emit_test_common.h"
#include "interpreter/casts.h"
#include <array>
#include <cmath>

using namespace Cbc;
using namespace Cbc::Format;
using namespace Interpretation;

class Float16EmitTest : public EmitTest {};

TEST(Float16, BitConversions)
{
    for (uint32_t bits = 0; bits <= 0xffff; ++bits) {
        ASSERT_EQ(Utils::Float16Bits(Utils::Float16FromBits(bits)), bits);
        ASSERT_EQ(Value::F16Bits(Value::MakeF16Bits(bits)), bits);
    }
}

TEST_F(Float16EmitTest, Float16RegisterAndMemoryTransport)
{
    Cbc::Emitter::Emitter e;
    e.Mov(FReg::FR0, IReg::IR2);
    e.Mov(FReg::FR1, FReg::FR0);
    e.FMovI16(FReg::FR0, Utils::Float16 { 42 }); // overwrite the input so the reload must supply the result
    e.StoreRec(StoreAccessKind::ST_F16, FReg::FR1, IReg::IR1, 1);
    e.LoadRec(LoadAccessKind::LD_F16, FReg::FR0, IReg::IR1, 1);
    e.Mov(IReg::IR1, FReg::FR0);
    e.BFXZ(IReg::IR1, IReg::IR1, 0, 16);
    e.Ret();
    auto code = e.Build(heap);
    const uint16_t samples[] {
        0x0000, // +0.0
        0x8000, // -0.0
        0x0001, // 2^-24
        0xbe00, // -1.5
        0x7bff, // 65504.0
        0x7c00, // +inf
        0xfc00, // -inf
        0x7e55, // quiet NaN
        0xfd55, // negative signaling NaN
    };
    for (uint16_t bits : samples) {
        std::array<uint8_t, 4> memory { 0xa5, 0x55, 0xaa, 0x5a };
        auto result =
            Interpret(code, U64(reinterpret_cast<uintptr_t>(memory.data())), U64(0xdeadbeef00000000ULL | bits));
        ASSERT_EQ(result.u64, bits);
        ASSERT_EQ(memory[0], 0xa5);
        ASSERT_EQ(memory[3], 0x5a);
    }
}

TEST_F(Float16EmitTest, Float16ImmediateAndRounding)
{
    Cbc::Emitter::Emitter e;
    e.FMovI16(FReg::FR0, Utils::Float16 { 2048 });
    e.FMovI16(FReg::FR1, Utils::Float16 { 1 });
    // the sum 2049 rounds to 2048 by ties-to-even, so the subtraction returns zero
    e.Binary(FloatOperations::FADD, Width::W16, FReg::FR0, FReg::FR0, FReg::FR1);
    e.FMovI16(FReg::FR1, Utils::Float16 { 2048 });
    e.Binary(FloatOperations::FSUB, Width::W16, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(Value::F16Bits(InterpretFPRes(code, U64(0), U64(0))), 0);
    EXPECT_NE(Cbc::Emitter::LogCode(code).find("fadd.16 FR0 FR0 FR1"), std::string::npos);
    EXPECT_NE(Cbc::Emitter::LogCode(code).find("fmovi.16 FR0 bits=0x6800"), std::string::npos);
}

TEST(Float16, NumericRoundTrips)
{
    for (uint32_t bits = 0; bits <= 0xffff; ++bits) {
        auto half = Value::MakeF16Bits(bits);
        for (auto wide : { ConvertType::F32, ConvertType::F64 }) {
            auto result = CastPrim(ConvertType::F16, wide, CastPrim(wide, ConvertType::F16, half));
            // signless encodings above infinity are NaNs, whose payload may change in a cast
            if ((bits & 0x7fff) > 0x7c00) {
                ASSERT_GT(Value::F16Bits(result) & 0x7fff, 0x7c00);
            } else {
                ASSERT_EQ(Value::F16Bits(result), bits);
            }
        }
    }
}

TEST(Float16, DirectDoubleConversion)
{
    for (auto entry : { std::pair<double, uint16_t> { 1.0004882812500002, 0x3c01 },
                        { 1.0014648437499998, 0x3c01 },
                        { 65504, 0x7bff },
                        { 65520, 0x7c00 },
                        { 0x1p-24, 1 },
                        { 0x1p-25, 0 },
                        { -0.0, 0x8000 } }) {
        EXPECT_EQ(Value::F16Bits(CastPrim(ConvertType::F16, ConvertType::F64, F64(entry.first))), entry.second);
    }
}

TEST(Float16, IntegerBoundsAndCheckedCasts)
{
    for (auto to : { ConvertType::I8,
                     ConvertType::U8,
                     ConvertType::I16,
                     ConvertType::U16,
                     ConvertType::I32,
                     ConvertType::U32,
                     ConvertType::I64,
                     ConvertType::U64 }) {
        EXPECT_EQ(CastPrim(to, ConvertType::F16, Value::MakeF16Bits(0x7e55 /* quiet NaN */)).u64, 0);
        EXPECT_EQ(CastPrim(to, ConvertType::F16, Value::MakeF16Bits(0x3f00 /* 1.75 */)).u64, 1);
        EXPECT_EQ(
            CheckedCastPrim(to, ConvertType::F16, Value::MakeF16Bits(0x7c00 /* +inf */)).exception,
            Type::CastOverflowInfOrNaN
        );
    }
    EXPECT_EQ(CastPrim(ConvertType::I8, ConvertType::F16, Value::MakeF16Bits(0x7c00 /* +inf */)).u64, 127);
    EXPECT_EQ(CastPrim(ConvertType::U16, ConvertType::F16, Value::MakeF16Bits(0x7bff /* 65504.0 */)).u64, 65504);
    EXPECT_EQ(CastPrim(ConvertType::U64, ConvertType::F16, Value::MakeF16Bits(0xbc00 /* -1.0 */)).u64, 0);
    EXPECT_EQ(
        CastPrim(ConvertType::I64, ConvertType::F16, Value::MakeF16Bits(0xfc00 /* -inf */)).u64, uint64_t(INT64_MIN)
    );
}

TEST(Float16, AggregatePieceKinds)
{
    CAbiField fields[] { { 0, 2, true }, { 2, 2, true }, { 4, 2, true } };
    auto singleHalf = ClassifyCAbiStruct({ fields, 1 }, 2, false, 8, 8);
    ASSERT_EQ(singleHalf.passBy, CAbiPassBy::REGS);
    ASSERT_EQ(singleHalf.pieces.Size(), 1);
    EXPECT_EQ(singleHalf.pieces[0].kind, CbcTypeKind::F16);

    auto aggregate = ClassifyCAbiStruct({ fields, 3 }, 6, false, 8, 8);
    ASSERT_EQ(aggregate.passBy, CAbiPassBy::REGS);
    if (HostTraits::C_ABI == CAbi::SYSV) {
        ASSERT_EQ(aggregate.pieces.Size(), 1);
        EXPECT_EQ(aggregate.pieces[0].kind, CbcTypeKind::F64);
    } else {
        ASSERT_EQ(aggregate.pieces.Size(), 3);
        for (int i = 0; i < 3; ++i) {
            EXPECT_EQ(aggregate.pieces[i].offset, 2 * i);
            EXPECT_EQ(aggregate.pieces[i].kind, CbcTypeKind::F16);
        }
    }
    EXPECT_EQ(ClassifyCAbiStruct({ fields, 3 }, 6, false, 8, 0).passBy, CAbiPassBy::STACK);

    CAbiField integers[] { { 0, 2, false }, { 2, 2, false }, { 4, 2, false } };
    auto integerAggregate = ClassifyCAbiStruct({ integers, 3 }, 6, false, 8, 8);
    ASSERT_EQ(integerAggregate.passBy, CAbiPassBy::REGS);
    ASSERT_EQ(integerAggregate.pieces.Size(), 1);
    EXPECT_EQ(integerAggregate.pieces[0].kind, CbcTypeKind::I64);
}

TEST(Float16, SerializedEncodingAndDisassembly)
{
    static_assert(Opcode::Float16 == 174 && Opcode::FMov16 == 175 && Opcode::FMov16i == 176);
    static_assert(Opcode::Bcc16 == 177 && Opcode::Scc16 == 178 && Opcode::FMathUnary16 == 179);
    static_assert(LoadAccessKind::LD_F16 == 9 && StoreAccessKind::ST_F16 == 8);
    uint8_t bytes[] { 176, 2, 0x55, 0xfd, 71, 0x92 };
    Stream::StringBuffer stream;
    RawDisasm(stream, bytes, bytes + sizeof(bytes));
    EXPECT_EQ(stream.ToString(), "0: fmovi.16 FR2 bits=64853\n4: fret.W16 FR2\n");
}

TEST_F(Float16EmitTest, CheckedHalfConversionsThrow)
{
    for (auto to : { ConvertType::I8,
                     ConvertType::U8,
                     ConvertType::I16,
                     ConvertType::U16,
                     ConvertType::I32,
                     ConvertType::U32,
                     ConvertType::I64,
                     ConvertType::U64 }) {
        heap.Reset();
        Cbc::Emitter::Emitter e;
        e.FMovI16(FReg::FR0, Utils::Float16FromBits(0x7e55 /* quiet NaN */));
        e.CheckedConvert(to, ConvertType::F16, IReg::IR1, FReg::FR0);
        e.Ret();
        EXPECT_EQ(InterpretThrows(e.Build(heap), U64(0), U64(0)), Type::CastOverflowInfOrNaN);
    }
}

TEST_F(Float16EmitTest, Float16BranchLongFixup)
{
    Cbc::Emitter::Emitter e;
    auto end = e.NewLabel();
    e.FMovI16(FReg::FR0, Utils::Float16FromBits(0x7e55 /* quiet NaN */));
    e.Bcc(CC::FNE, Width::W16, FReg::FR0, FReg::FR0, end);
    for (int i = 0; i < 2200; ++i)
        e.Mov(IReg::IR1, IReg::IR2);
    e.Bind(end);
    e.MovImm(Width::W64, IReg::IR1, 42);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 42);
}
