#include "cbc_test_harness.h"

// ---------------------------------------------------------------------------
// checks-misc: explicit checks, no-ops, float immediates, mov round-trips
// ---------------------------------------------------------------------------

TEST_ASM(CbcTest, NullCheckOk)
{
    uint64_t obj = 0;
    auto ref     = reinterpret_cast<uint64_t>(&obj);
    auto code    = OpenAndRewrite("test_nullcheck", "./generated-asm/checks-misc/nullcheck.asm")->code;
    EXPECT_EQ(InterpretThrows(code, U64(ref), U64(0)), Interpretation::Type::Successful);
    EXPECT_EQ(Interpret(code, U64(ref), U64(0)).u64, 1);
}

TEST_ASM(CbcTest, NullCheckFail)
{
    auto code = OpenAndRewrite("test_nullcheck", "./generated-asm/checks-misc/nullcheck.asm")->code;
    EXPECT_EQ(InterpretThrows(code, U64(0), U64(0)), Interpretation::Type::NoneValueException);
}

TEST_ASM(CbcTest, DivCheckOk)
{
    auto code = OpenAndRewrite("test_divcheck", "./generated-asm/checks-misc/divcheck.asm")->code;
    EXPECT_EQ(InterpretThrows(code, U64(5), U64(0)), Interpretation::Type::Successful);
    EXPECT_EQ(Interpret(code, U64(5), U64(0)).u64, 2);
}

TEST_ASM(CbcTest, DivCheckFail)
{
    auto code = OpenAndRewrite("test_divcheck", "./generated-asm/checks-misc/divcheck.asm")->code;
    EXPECT_EQ(InterpretThrows(code, U64(0), U64(0)), Interpretation::Type::ArithmeticException);
}

TEST_ASM(CbcTest, NopGcPoint)
{
    auto code = OpenAndRewrite("test_nop_gcpoint", "./generated-asm/checks-misc/nop-gcpoint.asm")->code;
    EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 3);
}

struct FloatImmCase {
    const char* name;
    uint64_t bits;
};

// The case sets must stay in sync with FLOAT_IMM32_LITERALS/FLOAT_IMM64_LITERALS in
// test/asm-generator/asm-generator.py. Expected values are the raw bit patterns
// (sign of -0.0 and every payload bit included). The inf/neginf cases still go
// through the overflowing-decimal-literal path; the nan cases use the word
// spelling accepted in the fmovi immediate position (assembler e08314ae), and
// the JVM encodes Double.NaN/Float.NaN as the canonical quiet NaN
// (0x7ff8000000000000/0x7fc00000, Assembler.scala fmovi: doubleToRawLongBits /
// floatToRawIntBits). The word inf is exercised by the FloatNegInf* tests.
static const FloatImmCase FloatImm32Cases[] = {
    { "zero", 0x00000000 },   { "negzero", 0x80000000 },   { "one", 0x3fc00000 },  { "inf", 0x7f800000 },
    { "neginf", 0xff800000 }, { "denormmin", 0x00000001 }, { "denormmax", 0x007fffff }, { "max", 0x7f7fffff },
    { "nan", 0x7fc00000 },
};

static const FloatImmCase FloatImm64Cases[] = {
    { "zero", 0x0000000000000000 },  { "negzero", 0x8000000000000000 },    { "one", 0x3ff8000000000000 },
    { "inf", 0x7ff0000000000000 },   { "neginf", 0xfff0000000000000 },     { "denormmin", 0x0000000000000001 },
    { "denormmax", 0x000fffffffffffff }, { "max", 0x7fefffffffffffff },    { "minnormal", 0x0010000000000000 },
    { "nan", 0x7ff8000000000000 },
};

TEST_ASM(CbcTest, FloatImm32)
{
    for (const auto& c : FloatImm32Cases) {
        auto path = std::string("./generated-asm/checks-misc/fmovi-32/") + c.name + ".asm";
        SCOPED_TRACE(path);
        auto code = OpenAndRewrite("test_" + std::string(c.name), path)->code;
        EXPECT_EQ(InterpretFPRes(code, F32(0), F32(0)).u32, static_cast<uint32_t>(c.bits));
    }
}

TEST_ASM(CbcTest, FloatImm64)
{
    for (const auto& c : FloatImm64Cases) {
        auto path = std::string("./generated-asm/checks-misc/fmovi-64/") + c.name + ".asm";
        SCOPED_TRACE(path);
        auto code = OpenAndRewrite("test_" + std::string(c.name), path)->code;
        EXPECT_EQ(InterpretFPRes(code, F64(0), F64(0)).u64, c.bits);
    }
}

// -inf has no literal spelling (dropped upstream, 75f971d7): modeled as
// inf x (-1.0). FMUL computes in float/double per width
// (src/interpreter/internal/operations.h:569/554), and x * (-1.0) flips only
// the sign bit of infinity.
TEST_ASM(CbcTest, FloatNegInf32)
{
    auto code = OpenAndRewrite("test_fmovi_neginf32", "./generated-asm/checks-misc/fmovi-neginf-32.asm")->code;
    EXPECT_EQ(InterpretFPRes(code, F32(0), F32(0)).u32, 0xff800000u);
}

TEST_ASM(CbcTest, FloatNegInf64)
{
    auto code = OpenAndRewrite("test_fmovi_neginf64", "./generated-asm/checks-misc/fmovi-neginf-64.asm")->code;
    EXPECT_EQ(InterpretFPRes(code, F64(0), F64(0)).u64, 0xfff0000000000000ull);
}

TEST_ASM(CbcTest, MovForms)
{
    {
        auto code = OpenAndRewrite("test_mov32", "./generated-asm/checks-misc/mov-32.asm")->code;
        // mov.32/mov.64 lower to the width-less RT MOV, a full 64-bit cell copy.
        for (uint64_t v : { 0ull, 1ull, 0x7fffffffull, 0x80000000ull, 0xffffffffull, 0x123456789ull, 0xffffffffffffffffull }) {
            SCOPED_TRACE("mov32 v=" + hex(v));
            EXPECT_EQ(Interpret(code, U64(v), U64(0)).u64, v);
        }
    }
    {
        auto code = OpenAndRewrite("test_mov64", "./generated-asm/checks-misc/mov-64.asm")->code;
        for (uint64_t v : { 0ull, 1ull, 0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull }) {
            SCOPED_TRACE("mov64 v=" + hex(v));
            EXPECT_EQ(Interpret(code, U64(v), U64(0)).u64, v);
        }
    }
    {
        auto code = OpenAndRewrite("test_movi32", "./generated-asm/checks-misc/movi-32.asm")->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u32, 0xau);
    }
    {
        auto code = OpenAndRewrite("test_movi32_narrow", "./generated-asm/checks-misc/movi-32-narrow.asm")->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u32, 0x7ffu);
    }
    {
        auto code = OpenAndRewrite("test_movi32_wide", "./generated-asm/checks-misc/movi-32-wide.asm")->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u32, 0x12345678u);
    }
    {
        uint64_t obj = 0;
        auto ref     = reinterpret_cast<uint64_t>(&obj);
        auto code    = OpenAndRewrite("test_movref", "./generated-asm/checks-misc/mov-ref.asm")->code;
        EXPECT_EQ(Interpret(code, U64(ref), U64(0)).u64, ref);
    }
}

struct MoviImmCase {
    const char* name;
    uint64_t bits;
};

// Negative movi.32/movi.64 immediates, round-tripped through ret. The DSL has
// no movi.8/movi.16 spellings (assembler table NewAsmParser.scala:897-898), and
// Emitter::MovImm accepts W32/W64 only (src/cbc/emitter/emitter.cpp:534).
// Case names must stay in sync with MOVI_NEG32_IMMS/MOVI_NEG64_IMMS in
// test/asm-generator/asm-generator.py. The immediates sweep the three
// post-rewrite encodings chosen by the signed immediate width: [-8,-1] lowers
// to RT MOVI (4-bit), [-2048,-9] to BINI32I/BINI64I (12-bit, via AddI from
// IRZ), anything below to BINI32L/BINI64L (literal pool) — Emitter::MovImm
// (emitter.cpp:532-543) and Emitter::BinaryImm (emitter.cpp:286-311). The
// rewriter merges the .cbc immediate into uint64 explicitly
// (src/cbc/isa_parser.cpp:334-338, post-8049baee).
//
// Expectations: MOVI sign-extends its 4-bit immediate into the full cell
// width-lessly (src/interpreter/interpretation_loop.cpp:294-300), so even the
// W32 forms hold 0xffff...ffff-patterns readable via res.u64; the 12-bit and
// literal paths feed Arith ADD (sign-extended 12-bit imm,
// src/interpreter/internal/operations.h:747-750, or the literal pool,
// operations.h:752-755), whose W32 result writes .u32 leaving the cell's upper
// half unspecified — those are compared via res.u32 per the harness convention.
static const MoviImmCase MoviImm32Cases[] = {
    { "neg1", 0xffffffffull },    { "neg8", 0xfffffff8ull },  { "neg9", 0xfffffff7ull },
    { "neg80", 0xffffff80ull },   { "neg800", 0xfffff800ull }, { "neg801", 0xfffff7ffull },
    { "neg8000", 0xffff8000ull }, { "negmin", 0x80000000ull },
};

static const MoviImmCase MoviImm64Cases[] = {
    { "neg1", 0xffffffffffffffffull },  { "neg8", 0xfffffffffffffff8ull }, { "neg9", 0xfffffffffffffff7ull },
    { "neg80", 0xffffffffffffff80ull }, { "neg800", 0xfffffffffffff800ull }, { "neg801", 0xfffffffffffff7ffull },
    { "neg8000", 0xffffffffffff8000ull }, { "negmin", 0x8000000000000000ull },
};

TEST_ASM(CbcTest, MoviNegImm32)
{
    for (const auto& c : MoviImm32Cases) {
        auto path = std::string("./generated-asm/checks-misc/movi-neg-32/") + c.name + ".asm";
        SCOPED_TRACE(path);
        auto code = OpenAndRewrite("test_" + std::string(c.name), path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u32, static_cast<uint32_t>(c.bits));
    }
    // The 4-bit MOVI path is width-less: it sign-extends into the full cell
    // even for the W32 spellings (interpretation_loop.cpp:294-300), unlike the
    // 12-bit/literal W32 paths whose cell upper half is unspecified.
    {
        auto code = OpenAndRewrite("test_neg1", "./generated-asm/checks-misc/movi-neg-32/neg1.asm")->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 0xffffffffffffffffull);
    }
    {
        auto code = OpenAndRewrite("test_neg8", "./generated-asm/checks-misc/movi-neg-32/neg8.asm")->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 0xfffffffffffffff8ull);
    }
}

TEST_ASM(CbcTest, MoviNegImm64)
{
    for (const auto& c : MoviImm64Cases) {
        auto path = std::string("./generated-asm/checks-misc/movi-neg-64/") + c.name + ".asm";
        SCOPED_TRACE(path);
        auto code = OpenAndRewrite("test_" + std::string(c.name), path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, c.bits);
    }
}
