#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>

#include "emit_test_common.h"

#include <cstring>

namespace Cbc {
namespace Emitter {

struct Test;

static Interpretation::Value::Primitive Ptr(void* p)
{
    return Interpretation::Value::Primitive { .u64 = reinterpret_cast<uintptr_t>(p) };
}

using namespace Cbc::Format;

// ---------------------------------------------------------------------------
// Closures, objects and boxes
// ---------------------------------------------------------------------------

TEST(EmitTest, InitClosureVariants)
{
    Emitter e;
    e.InitClosure(false);
    e.InitClosure(true);

    auto code = e.Build(heap);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE), code.bytecode[0]);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE_SRET), code.bytecode[1]);

    CHECK_DISASM(
        code,
        "0x000: init.closure",
        "0x001: init.closure.sret"
    );
}

TEST(EmitTest, NewObjGenericVariants)
{
    Emitter e;
    e.NewObjGeneric(IReg::IR2);
    e.NewObjGeneric(IReg::IR3, true);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: newobj.g IR2",
        "0x002: newobj.pinned.g IR3"
    );
}

TEST(EmitTest, NewBoxBuiltinType)
{
    Emitter e;
    e.NewBox(Interpretation::BUILTIN_RUNE);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: newbox 0xf");
}

// ---------------------------------------------------------------------------
// Simple arithmetic and moves
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Mov)
{
    Emitter e;
    e.MovImm(Width::W64, IReg::IR2, 0x7);
    e.Mov(IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: movi IR2 7",
        "0x002: mov IR1 IR2",
        "0x004: ret"
    );
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u64, 0x7);
}

TEST(EmitTest, Simple_MovF2I)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12345.678);
    e.Mov(IReg::IR1, FReg::FR0);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 12345.677734",
        "0x006: f2i IR1 FR0",
        "0x008: ret"
    );
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u32, 0x4640e6b6);
}

TEST(EmitTest, Simple_FMovI32)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 0.5);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 0.500000",
        "0x006: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 0.5);
}

TEST(EmitTest, Simple_FMovI64)
{
    Emitter e;
    e.FMovI64(FReg::FR10, 0.25);
    e.Mov(FReg::FR0, FReg::FR10);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.64 FR10 0.250000",
        "0x00a: fmov FR0 FR10",
        "0x00c: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f64, 0.25);
}

TEST(EmitTest, Simple_ArithB2rr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR1 IR2",
        "0x003: ret"
    );
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB3xrrr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR2, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR2 IR1",
        "0x003: ret"
    );
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB4xi12rr)
{
    Emitter e;
    e.AddI(Width::W32, IReg::IR1, IReg::IR2, 0xff);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 0xff00); // through literal
    e.BinaryImm(Common::SUB, Width::W32, IReg::IR1, IReg::IR1, 0xfff);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR2 255",
        "0x004: add.32 IR1 IR1 65280",
        "0x008: sub.32 IR1 IR1 4095",
        "0x00c: ret"
    );
    auto res = Interpret(code, U32(0), U32(1));
    EXPECT_EQ(res.u32, 0xf001);
}

TEST(EmitTest, Simple_ArithFP)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12.5);
    e.FMovI32(FReg::FR1, 2.5);
    e.FMovI32(FReg::FR2, 1.25);
    e.FMovI32(FReg::FR3, 0.25);
    e.Binary(FloatOperations::FADD, Width::W32, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Binary(FloatOperations::FSUB, Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Binary(FloatOperations::FDIV, Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Binary(FloatOperations::FMUL, Width::W32, FReg::FR0, FReg::FR0, FReg::FR3);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fmovi.32 FR0 12.500000",
        "0x006: fmovi.32 FR1 2.500000",
        "0x00c: fmovi.32 FR2 1.250000",
        "0x012: fmovi.32 FR3 0.250000",
        "0x018: fadd.32 FR0 FR0 FR1",
        "0x01b: fsub.32 FR0 FR0 FR2",
        "0x01e: fdiv.32 FR0 FR0 FR2",
        "0x021: fmul.32 FR0 FR0 FR3",
        "0x024: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 2.75);
}

// ---------------------------------------------------------------------------
// Branches and jumps
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Bcc)
{
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, label);
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Bind(label);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: bcc.32 eq IR1 IR2 3",
        "0x004: add.32 IR1 IR1 IR2",
        "0x007: ret"
    );

    auto res = Interpret(code, U32(2), U32(2));
    EXPECT_EQ(res.u32, 2);

    auto res2 = Interpret(code, U32(2), U32(1));
    EXPECT_EQ(res2.u32, 3);
}

TEST(EmitTest, Simple_Bcc_Loop)
{
    Emitter e;
    auto loop = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.Bcc(CC::LT, Width::W32, IReg::IR1, IReg::IR2, loop);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: add.32 IR1 IR1 1",
        "0x004: bcc.32 lt IR1 IR2 -8",
        "0x008: ret"
    );

    auto res = Interpret(code, U32(0), U32(100));
    EXPECT_EQ(res.u32, 100);
}

TEST(EmitTest, Simple_Jmp)
{
    Emitter e;
    auto l1 = e.NewLabel();
    auto l2 = e.NewLabel();
    auto l3 = e.NewLabel();
    e.Jmp(l2);
    e.MovImm(Width::W32, IReg::IR1, 1);
    e.Bind(l1);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 10);
    e.Jmp(l3);
    e.MovImm(Width::W32, IReg::IR1, 2);
    e.Bind(l2);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 20);
    e.Jmp(l1);
    e.MovImm(Width::W32, IReg::IR1, 3);
    e.Bind(l3);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 30);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: jmp 13",
        "0x005: movi IR1 1",
        "0x007: add.32 IR1 IR1 10",
        "0x00b: jmp 13",
        "0x010: movi IR1 2",
        "0x012: add.32 IR1 IR1 20",
        "0x016: jmp -20",
        "0x01b: movi IR1 3",
        "0x01d: add.32 IR1 IR1 30",
        "0x021: ret"
    );

    auto res = Interpret(code, U32(100), U32(0));
    EXPECT_EQ(res.u32, 160);
}

// ---------------------------------------------------------------------------
// Unary operations and float math
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Neg)
{
    Emitter e;
    e.Binary(Common::SUB, Width::W32, IReg::IR1, IReg::IRZ, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: sub.32 IR1 IRZ IR2",
        "0x003: ret"
    );

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ((int32_t)res.u32, -10);
}

TEST(EmitTest, Simple_FNeg)
{
    Emitter e;
    e.Unary(FloatOperations::FNEG, Width::W32, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fneg.32 FR0 FR1",
        "0x003: ret"
    );

    float arg1 = 1.2;
    auto res1  = InterpretFPRes(code, F32(0), F32(arg1));
    EXPECT_EQ(res1.f32, -arg1);

    float arg2 = +0.0;
    auto res2  = InterpretFPRes(code, F32(0), F32(arg2));
    EXPECT_EQ(res2.f32, -arg2);
}

TEST(EmitTest, Simple_FSqrt)
{
    Emitter e;
    e.Unary(FloatOperations::FSQRT, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fsqrt.64 FR0 FR1",
        "0x003: ret"
    );

    double arg = 16.81;
    auto res   = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::sqrt(arg));
}

TEST(EmitTest, Simple_FAbs)
{
    Emitter e;
    e.Unary(FloatOperations::FABS, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fabs.64 FR0 FR1",
        "0x003: ret"
    );

    double arg1 = -1.2;
    auto res1   = InterpretFPRes(code, F64(0), F64(arg1));
    EXPECT_EQ(res1.f64, std::fabs(arg1));

    double arg2 = -0.0;
    auto res2   = InterpretFPRes(code, F64(0), F64(arg2));
    EXPECT_EQ(res2.f64, +0.0);
}

// ---------------------------------------------------------------------------
// Parameterized float math: (op, value) pairs
// ---------------------------------------------------------------------------

struct FloatMathUnaryCase {
    FloatMathOp op;
};

FloatMathUnaryCase floatMathUnaryCases[] = {
    { FloatMathOp::SIN },   { FloatMathOp::COS },   { FloatMathOp::LOG },  { FloatMathOp::CEIL },
    { FloatMathOp::FLOOR }, { FloatMathOp::LOG10 }, { FloatMathOp::LOG2 }, { FloatMathOp::EXP },
    { FloatMathOp::EXP2 },  { FloatMathOp::TRUNC }
};

double floatMathUnaryValues64[] = {
    4.2,     -4.2,  2.0,    -2.0,  0.5,  -0.5, 1.0,  -1.0,   3.0,  10.0, 100.0, 0.1, 1024.0, 123456.789,
    1e-10,   1e10,  1e-300, 1e300, 1e17, 0.0,  -0.0, 709.0, -709.0, 1023.0,
    std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
    std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
};

float floatMathUnaryValues32[] = {
    4.2f,    -4.2f, 2.0f,   -2.0f, 0.5f, -0.5f, 1.0f, -1.0f, 3.0f, 10.0f, 100.0f, 0.1f, 1024.0f, 123456.789f,
    1e-10f,  1e10f, 1e-30f, 1e30f, 0.0f, -0.0f, 88.0f, -88.0f,
    std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
    std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()
};

struct FPowiCase {
    double base;
    int64_t exponent;
};

FPowiCase fpowiCases[] = {
    { 2.0, 10 }, { 3.0, -2 }, { 5.0, 0 }, { 2.0, 63 }, { 0.5, -1 }, { 2.0, -63 }, { -2.0, 3 }, { -2.0, 4 },
    { 1.5, 100 }, { 0.0, 5 }, { 0.0, 0 }, { 2.0, INT64_MAX }, { 2.0, INT64_MIN }
};

std::pair<double, double> fpowCases[] = {
    { 2.0, 10.0 },
    { 3.0, -2.0 },
    { 4.2, 7.3 },
    { 0.0, 0.0 },
    { 0.0, -1.0 },
    { 0.0, 2.5 },
    { -2.0, 3.0 },
    { -2.0, 0.5 },
    { 1.0, std::numeric_limits<double>::quiet_NaN() },
    { std::numeric_limits<double>::quiet_NaN(), 0.0 },
    { std::numeric_limits<double>::infinity(), 0.0 },
    { -std::numeric_limits<double>::infinity(), 2.0 },
    { 2.0, std::numeric_limits<double>::infinity() },
    { 2.0, -std::numeric_limits<double>::infinity() },
    { 0.5, -1.0 },
    { std::numeric_limits<double>::max(), 2.0 },
    { std::numeric_limits<double>::max(), -2.0 },
    { -8.0, 1.0 / 3.0 },
    { 1e-300, 2.0 },
    { 123.456, 1.5 }
};

double FPMathUnaryExpected64(FloatMathOp op, double v)
{
    switch (op) {
        case FloatMathOp::SIN:   return std::sin(v);
        case FloatMathOp::COS:   return std::cos(v);
        case FloatMathOp::LOG:   return std::log(v);
        case FloatMathOp::CEIL:  return std::ceil(v);
        case FloatMathOp::FLOOR: return std::floor(v);
        case FloatMathOp::LOG10: return std::log10(v);
        case FloatMathOp::LOG2:  return std::log2(v);
        case FloatMathOp::EXP:   return std::exp(v);
        case FloatMathOp::EXP2:  return std::exp2(v);
        case FloatMathOp::TRUNC: return std::trunc(v);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

float FPMathUnaryExpected32(FloatMathOp op, float v)
{
    switch (op) {
        case FloatMathOp::SIN:   return std::sin(v);
        case FloatMathOp::COS:   return std::cos(v);
        case FloatMathOp::LOG:   return std::log(v);
        case FloatMathOp::CEIL:  return std::ceil(v);
        case FloatMathOp::FLOOR: return std::floor(v);
        case FloatMathOp::LOG10: return std::log10(v);
        case FloatMathOp::LOG2:  return std::log2(v);
        case FloatMathOp::EXP:   return std::exp(v);
        case FloatMathOp::EXP2:  return std::exp2(v);
        case FloatMathOp::TRUNC: return std::trunc(v);
    }
    return std::numeric_limits<float>::quiet_NaN();
}

class EmitFloatMathUnary64 :
    public EmitTest, public ::testing::WithParamInterface<std::tuple<FloatMathUnaryCase, double>> {
};

class EmitFloatMathUnary32 :
    public EmitTest, public ::testing::WithParamInterface<std::tuple<FloatMathUnaryCase, float>> {
};

class EmitFPow : public EmitTest, public ::testing::WithParamInterface<std::pair<double, double>> {};

class EmitFPowi : public EmitTest, public ::testing::WithParamInterface<FPowiCase> {};

std::string FloatMathUnaryName64(const ::testing::TestParamInfo<std::tuple<FloatMathUnaryCase, double>>& info)
{
    return std::string(std::get<0>(info.param).op.CStr()) + "_v" + Hex(F64(std::get<1>(info.param)).u64);
}

std::string FloatMathUnaryName32(const ::testing::TestParamInfo<std::tuple<FloatMathUnaryCase, float>>& info)
{
    return std::string(std::get<0>(info.param).op.CStr()) + "_v" + Hex(F32(std::get<1>(info.param)).u32);
}

std::string FPowCaseName(const ::testing::TestParamInfo<std::pair<double, double>>& info)
{
    return "v" + Hex(F64(info.param.first).u64) + "_v" + Hex(F64(info.param.second).u64);
}

std::string FPowiCaseName(const ::testing::TestParamInfo<FPowiCase>& info)
{
    int64_t exponent   = info.param.exponent;
    uint64_t magnitude = exponent < 0 ? 0ull - static_cast<uint64_t>(exponent) : static_cast<uint64_t>(exponent);
    std::string suffix = exponent < 0 ? "neg" + std::to_string(magnitude) : std::to_string(magnitude);
    return "v" + Hex(F64(info.param.base).u64) + "_e" + suffix;
}

TEST_P(EmitFloatMathUnary64, Computes)
{
    auto [c, value]  = GetParam();
    FloatMathOp op   = c.op;
    Emitter e;
    e.FMathUnary(op, Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: " + std::string(op.CStr()) + ".64 FR0 FR1",
        "0x003: ret"
    );
    auto res = InterpretFPRes(code, F64(0), F64(value));
    EXPECT_EQ(res.u64, F64(FPMathUnaryExpected64(op, value)).u64);
}

TEST_P(EmitFloatMathUnary32, Computes)
{
    auto [c, value]  = GetParam();
    FloatMathOp op   = c.op;
    Emitter e;
    e.FMathUnary(op, Width::W32, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: " + std::string(op.CStr()) + ".32 FR0 FR1",
        "0x003: ret"
    );
    auto res = InterpretFPRes(code, F32(0), F32(value));
    EXPECT_EQ(res.u32, F32(FPMathUnaryExpected32(op, value)).u32);
}

TEST_P(EmitFPow, Computes64)
{
    auto [l, r] = GetParam();
    Emitter e;
    e.Binary(FloatOperations::FPOW, Width::W64, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fpow.64 FR0 FR0 FR1",
        "0x003: ret"
    );
    auto res = InterpretFPRes(code, F64(l), F64(r));
    EXPECT_EQ(res.u64, F64(std::pow(l, r)).u64);
}

TEST_P(EmitFPow, Computes32)
{
    auto [l, r] = GetParam();
    Emitter e;
    e.Binary(FloatOperations::FPOW, Width::W32, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fpow.32 FR0 FR0 FR1",
        "0x003: ret"
    );
    auto res = InterpretFPRes(code, F32(static_cast<float>(l)), F32(static_cast<float>(r)));
    EXPECT_EQ(res.u32, F32(std::pow(static_cast<float>(l), static_cast<float>(r))).u32);
}

TEST_P(EmitFPowi, Computes64)
{
    auto c = GetParam();
    Emitter e;
    e.FPowi(Width::W64, FReg::FR0, FReg::FR1, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fpowi.64 FR0 FR1 IR1",
        "0x003: ret"
    );
    auto res = InterpretFPRes(code, U64(c.exponent), U64(0), F64(0), F64(c.base));
    EXPECT_EQ(res.u64, F64(std::pow(c.base, static_cast<double>(c.exponent))).u64);
}

TEST_P(EmitFPowi, Computes32)
{
    auto c = GetParam();
    Emitter e;
    e.FPowi(Width::W32, FReg::FR0, FReg::FR1, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: fpowi.32 FR0 FR1 IR1",
        "0x003: ret"
    );
    auto base = static_cast<float>(c.base);
    auto res  = InterpretFPRes(code, U64(c.exponent), U64(0), F32(0), F32(base));
    EXPECT_EQ(res.u32, F32(static_cast<float>(std::pow(static_cast<double>(base), static_cast<double>(c.exponent)))).u32);
}

INSTANTIATE_TEST_SUITE_P(
    EmitTest,
    EmitFloatMathUnary64,
    ::testing::Combine(::testing::ValuesIn(floatMathUnaryCases), ::testing::ValuesIn(floatMathUnaryValues64)),
    FloatMathUnaryName64
);

INSTANTIATE_TEST_SUITE_P(
    EmitTest,
    EmitFloatMathUnary32,
    ::testing::Combine(::testing::ValuesIn(floatMathUnaryCases), ::testing::ValuesIn(floatMathUnaryValues32)),
    FloatMathUnaryName32
);

INSTANTIATE_TEST_SUITE_P(EmitTest, EmitFPow, ::testing::ValuesIn(fpowCases), FPowCaseName);

INSTANTIATE_TEST_SUITE_P(EmitTest, EmitFPowi, ::testing::ValuesIn(fpowiCases), FPowiCaseName);

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

TEST(EmitTest, Simple_Convert)
{
    Emitter e;
    e.Convert(ConvertType::U8, ConvertType::U32, IReg::IR1, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: convert U8 U32 IR1 IR1",
        "0x003: ret"
    );

    auto res = Interpret(code, U32(32896), U32(0), F32(0), F32(0));
    EXPECT_EQ(res.u64, U64(128).u64);
}

// ---------------------------------------------------------------------------
// Emitter state: labels, address symbols
// ---------------------------------------------------------------------------

TEST(EmitTest, LabelPosition)
{
    Emitter e;
    e.Nop();
    auto label = e.NewLabel();
    e.Bind(label);
    EXPECT_EQ(e.LabelPosition(label), 1);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: nop");
}

TEST(EmitTest, AddressSymbolsAndStaticAccess)
{
    Emitter e;
    uint64_t value1 = 0xAAAA1111AAAA1111ull;
    uint64_t value2 = 0xBBBB2222BBBB2222ull;
    auto sym1       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value1));
    auto sym2       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value2));
    e.LoadStatic(LoadAccessKind::LD_64, IReg::IR1, sym1);
    e.StoreStatic(StoreAccessKind::ST_16, IReg::IR2, sym2);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, reinterpret_cast<uintptr_t>(&value1));
    EXPECT_EQ(code.literals->at(1).u64, reinterpret_cast<uintptr_t>(&value2));

    // LOAD_ADDR prints the load access kind correctly; the U16L operand prints
    // the literal-table address (STORE_ADDR is checked in
    // Disasm_StoreAddrPrintsStoreAccessKind).
    std::string prefix = "0x000: ld.addr.64 IR1 0x";
    EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
}

TEST(EmitTest, Disasm_StoreAddrPrintsStoreAccessKind)
{
    Emitter e;
    uint64_t value = 0;
    auto sym       = e.NewAddressSym(reinterpret_cast<uintptr_t>(&value));
    e.StoreStatic(StoreAccessKind::ST_16, IReg::IR2, sym);
    e.StoreStatic(StoreAccessKind::ST_64, IReg::IR3, sym);

    auto code = e.Build(heap);
    // Contract: st.addr.$0stk — the store access kind must be printed with its
    // own name table ("16", "64"), like STORE_OBJ does ("st.$0stk"); the U16L
    // operand prints the literal-table address.
    auto addr = Hex(&value);
    CHECK_DISASM(
        code,
        "0x000: st.addr.16 IR2 " + addr,
        "0x004: st.addr.64 IR3 " + addr
    );
}

TEST(EmitTest, BranchIfRef)
{
    Emitter e;
    auto label = e.NewLabel();
    e.BranchIfRef(IReg::IR2, label);
    e.Ret();
    e.Bind(label);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: branch.is.ref IR2 1",
        "0x003: ret"
    );
}

// ---------------------------------------------------------------------------
// Small single-byte instructions
// ---------------------------------------------------------------------------
TEST(EmitTest, SmallOps)
{
    Emitter e;
    e.Nop();
    e.GcPoint();
    e.CallClosure(false);
    e.CallClosure(true);
    e.CallClosureGeneric();
    e.SpawnFuture();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: nop",
        "0x001: gcpoint",
        "0x002: call.closure",
        "0x003: call.closure.sret",
        "0x004: call.closure.g",
        "0x005: spawn.future"
    );
}

TEST(EmitTest, Spawn)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.Spawn(ti);
    e.SpawnFuture();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: spawn " + Hex(ti),
        "0x009: spawn.future"
    );
}

TEST(EmitTest, DirectCalls)
{
    Emitter e;
    e.DirectCall2i(e.NewAddressSym(42));
    e.DirectCall2c(e.NewAddressSym(77));

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, 42u);
    EXPECT_EQ(code.literals->at(1).u64, 77u);

    CHECK_DISASM(
        code,
        "0x000: call.2i 42",
        "0x003: call.2c 77"
    );
}

TEST(EmitTest, DirectCall2n)
{
    Emitter e;
    e.DirectCall2n(e.NewAddressSym(99), 8, 3);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 1u);
    EXPECT_EQ(code.literals->at(0).u64, 99u);

    CHECK_DISASM(code, "0x000: call.2n 99 (3, 0x8)");
}

TEST(EmitTest, CallCFunc)
{
    Emitter e;
    e.CallCFunc(IReg::IR2, 6, 1);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: call.cfunc IR2 (1, 0x6)");
}

TEST(EmitTest, VirtualCall)
{
    Emitter e;
    e.VirtualCall(5, 7, true);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: vcall 0x5 0x7");
}

TEST(EmitTest, InterfaceCall)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.InterfaceCall(5, ti, true, Interpretation::StaticCallTypeInfoArgs { 3, 4 });
    e.InterfaceCall(6, ti, false, Interpretation::StaticCallTypeInfoArgs {});

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: icall.0x1 0x5 " + Hex(ti) + " outer=0x3 this=0x4",
        "0x010: icall.0x0 0x6 " + Hex(ti) + " outer=0xffff this=0xffff"
    );
}

TEST(EmitTest, InterfaceCallGeneric)
{
    Emitter e;
    e.InterfaceCallGeneric(5, true, Interpretation::StaticCallTypeInfoArgs { 3, 4 });
    e.InterfaceCallGeneric(6, false, Interpretation::StaticCallTypeInfoArgs {});

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: icall.g.0x1 0x5 outer=0x3 this=0x4",
        "0x008: icall.g.0x0 0x6 outer=0xffff this=0xffff"
    );
}

TEST(EmitTest, StringLit)
{
    auto* storage = heap.New<Interpretation::StringStorage>();
    Emitter e;
    e.StringLit(storage, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: string.init " + Hex(storage) + " 0x8");
}

TEST(EmitTest, ChecksAndExceptions)
{
    Emitter e;
    e.DivCheck(IReg::IR2);
    e.NullCheck(IReg::IR3);
    e.Throw(IReg::IR4);
    e.Catch(IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: divcheck IR2",
        "0x002: nullcheck IR3",
        "0x004: throw IR4",
        "0x006: catch IR5"
    );
}

TEST(EmitTest, LoadGenericTypeInfo)
{
    Emitter e;
    e.LoadGenericTypeInfo(0x1234);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: load.generic.ti 0x1234");
}

TEST(EmitTest, InstanceOf)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.InstanceOf(IReg::IR1, IReg::IR2, ti);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: iof IR1 IR2 " + Hex(ti));
}

TEST(EmitTest, BoxOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.NewBox(ti);
    e.UnboxT(IReg::IR2, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: newbox2 " + Hex(ti),
        "0x009: unbox.t IR2 0x8"
    );
}

TEST(EmitTest, Disasm_OffsetPrintsDestination)
{
    Emitter e;
    e.Offset(IReg::IR3, 7, IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: offset IR3 IR5 0x7 }");
}

TEST(EmitTest, StructFieldOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.WriteStructField(IReg::IR4, IReg::IR5, 6, ti);
    e.ReadStructField(IReg::IR1, IReg::IR2, 8, ti);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: write.struct.field IR4 IR5 0x6 " + Hex(ti),
        "0x00b: read.struct.field IR1 IR2 0x8 " + Hex(ti)
    );
}

TEST(EmitTest, GenericAndNewOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.AssignGeneric(IReg::IR1, IReg::IR2, IReg::IR3);
    e.InstanceOfGeneric(IReg::IR1, IReg::IR2, IReg::IR3);
    e.NewObjGenericOnAcc(IReg::IR2);
    e.NewArrGeneric(IReg::IR2);
    e.NewArr(ti);
    e.NewObj(ti, true);
    e.TypeArg(IReg::IR1, IReg::IR2, 3);
    e.Mov(IReg::IR_ACC, IReg::IR1);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: assign.g IR1 IR2 IR3",
        "0x003: iof.g IR1 IR2 IR3",
        "0x006: newobj.acc.g IR2",
        "0x008: newarr.g IR1, IR2, IR2",
        "0x00a: newarr IR1, IR2, " + Hex(ti),
        "0x013: newobj.pinned IR1, " + Hex(ti),
        "0x01c: type.arg IR1 IR2 3",
        "0x020: mov IR_ACC IR1"
    );
}

// ---------------------------------------------------------------------------
// Long (32-bit offset) load/store variants
// ---------------------------------------------------------------------------
TEST(EmitTest, LoadStoreDerived_LongOffsets)
{
    Emitter e;
    e.LoadDerived(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_16, IReg::IR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_F32, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);
    e.StoreDerived(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.derived.64 IR1 [(IR2 IR3) 0x10000]",
        "0x007: st.derived.16 IR1 [(IR2 IR3) 0x10000]",
        "0x00e: st.derived.f32 FR1 [(IR2 IR3) 0x10000]",
        "0x015: st.derived.f64 FR1 [(IR2 IR3) 0x10000]"
    );
}

TEST(EmitTest, Disasm_LoadLongDerivedFPrintsFloatRegister)
{
    Emitter e;
    e.LoadDerived(LoadAccessKind::LD_F32, FReg::FR1, IReg::IR2, IReg::IR3, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(code, "0x000: ld.derived.f32 FR1 [(IR2 IR3) 0x10000]");
}

TEST(EmitTest, LoadStoreObj_LongOffsets)
{
    Emitter e;
    e.LoadObj(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, 0x10000);
    e.StoreObj(StoreAccessKind::ST_64, IReg::IR1, IReg::IR2, 0x10000);
    e.LoadObj(LoadAccessKind::LD_F64, FReg::FR1, IReg::IR2, 0x10000);
    e.StoreObj(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.derived.64 IR1 [(IR2 IR2) 0x10000]",
        "0x007: st.derived.64 IR1 [(IR2 IR2) 0x10000]",
        "0x00e: ld.derived.f64 FR1 [(IR2 IR2) 0x10000]",
        "0x015: st.derived.f64 FR1 [(IR2 IR2) 0x10000]"
    );
}

TEST(EmitTest, LoadStoreRec_LongOffsets)
{
    Emitter e;
    e.LoadRec(LoadAccessKind::LD_64, IReg::IR1, IReg::IR2, 0x10000);
    e.StoreRec(StoreAccessKind::ST_16, IReg::IR1, IReg::IR2, 0x10000);
    e.LoadRec(LoadAccessKind::LD_F32, FReg::FR1, IReg::IR2, 0x10000);
    e.StoreRec(StoreAccessKind::ST_F64, FReg::FR1, IReg::IR2, 0x10000);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.rec.64 IR1 [IR2 0x10000]",
        "0x007: st.rec.16 IR1 [IR2 0x10000]",
        "0x00e: ld.rec.f32 FR1 [IR2 0x10000]",
        "0x015: st.rec.f64 FR1 [IR2 0x10000]"
    );
}

TEST(EmitTest, LoadStoreFrame_LongOffsets)
{
    Emitter e;
    e.LoadFrame(LoadAccessKind::LD_64, IReg::IR1, 0x10000);
    e.StoreFrame(StoreAccessKind::ST_16, IReg::IR1, 0x10000);
    e.LoadFrame(LoadAccessKind::LD_F64, FReg::FR1, 0x10000);
    e.StoreFrame(StoreAccessKind::ST_F32, FReg::FR1, 0x10000);
    e.LoadFrame(LoadAccessKind::LD_64, IReg::IR1, -8); // short encoding with negative offset

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.frame.64 IR1 [65536]",
        "0x007: st.frame.16 IR1 [65536]",
        "0x00e: ld.frame.f64 FR1 [0x10000]",
        "0x015: st.frame.f32 FR1 [0x10000]",
        "0x01c: ld.frame.64 IR1 [IRZ -8]"
    );
}

// ---------------------------------------------------------------------------
// Array access
// ---------------------------------------------------------------------------
TEST(EmitTest, ArrayAccess)
{
    Emitter e;
    e.LoadArray(LoadAccessKind::LD_32, IReg::IR1, IReg::IR2, IReg::IR3);
    e.StoreArray(StoreAccessKind::ST_REF, IReg::IR1, IReg::IR2, IReg::IR3);
    e.LoadArray(LoadAccessKind::LD_F64, FReg::FR1, IReg::IR2, IReg::IR3);
    e.StoreArray(StoreAccessKind::ST_F32, FReg::FR1, IReg::IR2, IReg::IR3);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.arr.32 IR1 IR2 IR3",
        "0x003: st.arr.ref IR1 IR2 IR3]",
        "0x006: ld.arr.f64 FR1 IR2 IR3]",
        "0x009: st.arr.f32 FR1 IR2 IR3]"
    );
}

// ---------------------------------------------------------------------------
// Generic addressing
// ---------------------------------------------------------------------------
TEST(EmitTest, GenericAddressing)
{
    Emitter e;
    e.LoadGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4);
    e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 0);
    e.LeaGeneric(IReg::IR1, IReg::IR3, IReg::IR4, 5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ld.g IR1 IR4 [(IR2 IR3)]",
        "0x003: st.g0 IR1 IR2 IR3 IR4",
        "0x006: lea.g IR1 IR4 [IR3 ord=0x5]"
    );
}

// ---------------------------------------------------------------------------
// 32-byte block string helpers (scalar interpreter implementation)
// ---------------------------------------------------------------------------
TEST(EmitTest, Vidx32)
{
    Emitter e;
    e.Vidx32(IReg::IR1, IReg::IR1, IReg::IR2, IReg::IR3);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: vidx.32 IR1 IR1 IR2 IR3",
        "0x003: ret"
    );

    // Fake array object: data starts at reference + ArrayBodyOffset().
    auto mem  = static_cast<uint8_t*>(heap.Allocate(64, 16));
    auto data = mem + RTSupport::MetaInfo::ArrayBodyOffset();
    memset(data, 'a', 64 - RTSupport::MetaInfo::ArrayBodyOffset());
    data[17] = 'b';

    // Search for 'b' starting at offset 10: found at data index 17, i.e. at 7
    // relative to the offset.
    auto res = Interpret(code, Ptr(mem), U64(10), U64('b'));
    EXPECT_EQ(res.u64, 7);

    // Missing byte: the whole 32-byte window is scanned, result is 32.
    res = Interpret(code, Ptr(mem), U64(0), U64('z'));
    EXPECT_EQ(res.u64, 32);

    // First byte matches.
    res = Interpret(code, Ptr(mem), U64(0), U64('a'));
    EXPECT_EQ(res.u64, 0);
}

TEST(EmitTest, VCmp32)
{
    Emitter e;
    e.MovImm(Width::W64, IReg::IR4, 0); // off2 = 0
    e.Vcmp32(IReg::IR1, IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: movi IR4 0",
        "0x002: vcmp.32 IR1 IR1 IR2 IR3 IR4",
        "0x006: ret"
    );

    auto* mem1  = static_cast<uint8_t*>(heap.Allocate(64, 16));
    auto* mem2  = static_cast<uint8_t*>(heap.Allocate(64, 16));
    auto* data1 = mem1 + RTSupport::MetaInfo::ArrayBodyOffset();
    auto* data2 = mem2 + RTSupport::MetaInfo::ArrayBodyOffset();
    for (int i = 0; i < 32; i++) {
        data1[i] = static_cast<uint8_t>(i);
        data2[i] = static_cast<uint8_t>(i + 4); // data2[j] == data1[j + 4]
    }
    data2[10] = 0xFF;

    // Compare arr1 at offset 4 with arr2 at offset 0: equal until the poisoned
    // byte, so the first difference is at relative index 10.
    auto res = Interpret(code, Ptr(mem1), U64(4), Ptr(mem2));
    EXPECT_EQ(res.u64, 10);

    // Identical 32-byte windows compare equal: result is 32.
    res = Interpret(code, Ptr(mem1), U64(0), Ptr(mem1));
    EXPECT_EQ(res.u64, 32);
}

// ---------------------------------------------------------------------------
// TypeInfo queries: raw DYN_TypeInfo pointer operand, zero-extended results
// ---------------------------------------------------------------------------
TEST(EmitTest, TypeSize)
{
    auto ti = RTSupport::TypeInfo(heap.New<TestTypeInfo>(24, 8));
    Emitter e;
    e.TypeSize(IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ti.size IR1 IR2",
        "0x002: ret"
    );

    // Behavior-pinning: the result must equal MetaInfo::GetTypeSize for the
    // TypeInfo used, zero-extended to u64.
    auto res = Interpret(code, U64(0), Ptr(ti.Raw()));
    EXPECT_EQ(res.u64, RTSupport::MetaInfo::GetTypeSize(ti));
    EXPECT_EQ(res.u64, 24);
}

TEST(EmitTest, TypeAlign)
{
    auto ti = RTSupport::TypeInfo(heap.New<TestTypeInfo>(24, 8));
    Emitter e;
    e.TypeAlign(IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: ti.align IR1 IR2",
        "0x002: ret"
    );

    // Behavior-pinning: the result must equal MetaInfo::GetAlign for the
    // TypeInfo used, zero-extended to u64.
    auto res = Interpret(code, U64(0), Ptr(ti.Raw()));
    EXPECT_EQ(res.u64, RTSupport::MetaInfo::GetAlign(ti));
    EXPECT_EQ(res.u64, 8);
}

TEST(EmitTest, StoreGeneric_OffsetWidths)
{
    {
        Emitter e; // 1..254 -> STORE_GENERIC_1 with an 8-bit offset
        e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 254);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: st.g1 IR1 IR2 IR3 IR4 0xfe");
    }
    {
        Emitter e; // >= 255 -> STORE_GENERIC_4 with a 32-bit offset
        e.StoreGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 255);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: st.g4 IR1 IR2 IR3 IR4 0xff");
    }
}

TEST(EmitTest, InitGenericOption)
{
    {
        Emitter e;
        e.InitNoneGenericOpt0(IReg::IR1);

        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: init.none.g0 IR1");
    }
    {
        Emitter e;
        e.InitSomeGeneric(IReg::IR2, IReg::IR3, true);  // OPT0
        e.InitSomeGeneric(IReg::IR4, IReg::IR5, false); // OPT1

        auto code = e.Build(heap);
        CHECK_DISASM(
            code,
            "0x000: init.some.g0 IR2 IR3",
            "0x002: init.some.g1 IR4 IR5"
        );
    }
}

TEST(EmitTest, IndexAddressing)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.LeaIndex(IReg::IR1, IReg::IR2, IReg::IR3, ti, true);
    e.LeaIndex(IReg::IR1, IReg::IR2, IReg::IR3, ti, false);
    e.LeaIndexGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4);
    e.LeaConstIndexGeneric(IReg::IR1, IReg::IR2, -7, IReg::IR4);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: index IR1 [IR2 IR3] " + Hex(ti),
        "0x00b: index IR1 [IR2 IR3] " + Hex(ti),
        "0x016: index.g IR1 [IR2 IR3] IR4",
        "0x019: index.g IR1 [IR2 -7] IR4"
    );
}

TEST(EmitTest, CopyDerivedOps)
{
    auto ti = NewTestTypeInfo(16);
    Emitter e;
    e.CopyDerived(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, ti);
    e.CopyDerivedGeneric(IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, IReg::IR5);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: copy.derived IR1 IR2 IR3 IR4 " + Hex(ti),
        "0x00b: copy.derived.g IR1 IR2 IR3 IR4 IR5"
    );
}

// ---------------------------------------------------------------------------
// Atomic operations
// ---------------------------------------------------------------------------
TEST(EmitTest, AtomicLoadStore)
{
    Emitter e;
    e.AtomicLoad(IReg::IR1, LoadAccessKind::LD_64, IReg::IR2, 8);
    e.AtomicStore(IReg::IR1, StoreAccessKind::ST_32, IReg::IR2, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: atomic.ld.64 IR1 [IR2 0x8]",
        "0x004: atomic.st.32 IR1 [IR2 0x8]"
    );
}

TEST(EmitTest, Disasm_AtomicOpsPrintAllOperands)
{
    Emitter e;
    e.AtomicOp(RT::Opcode::ATOMIC_FETCH_ADD_64, IReg::IR1, IReg::IR2, IReg::IR3, 8);
    e.CAS(RT::Opcode::CAS_64, IReg::IR1, IReg::IR2, IReg::IR3, IReg::IR4, 8);
    e.AtomicOp(RT::Opcode::ATOMIC_SWAP_64, IReg::IR1, IReg::IR2, IReg::IR3, 8);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: atomic.fetch.add.64 IR1 IR3 [IR2 0x8]",
        "0x005: cas.64 IR1 IR3 IR4 [IR2 0x8]",
        "0x00a: atomic.swap.64 IR1 IR3 [IR2 0x8]"
    );
}

// ---------------------------------------------------------------------------
// Stack parameters
// ---------------------------------------------------------------------------
TEST(EmitTest, StackParams)
{
    Emitter e;
    e.StackParam(IReg::IR3, 16);
    e.StackParamF(FReg::FR3, 20);
    e.StackParamS16(-5, 24);
    e.StackParamS32(-100000, 28);

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: st.stack.param IR3 0x10",
        "0x004: st.stack.param.f FR3 0x14",
        "0x008: st.stack.param.s.16 -5 0x18",
        "0x00d: st.stack.param.s.32 -100000 0x1c"
    );
}

TEST(EmitTest, SCCImm_Literal)
{
    Emitter e;
    e.SCCImm(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, 1048576);
    e.SCCImm(CC::NE, Width::W64, IReg::IR1, IReg::IR2, 1048576);

    auto code = e.Build(heap);
    ASSERT_EQ(code.literals->size(), 2u);
    EXPECT_EQ(code.literals->at(0).u64, 1048576u);
    EXPECT_EQ(code.literals->at(1).u64, 1048576u);

    CHECK_DISASM(
        code,
        "0x000: scci.32 eq IR1 IR2 1048576",
        "0x004: scci.64 ne IR1 IR2 1048576"
    );
}

// ---------------------------------------------------------------------------
// Immediate frame stores of every size
// ---------------------------------------------------------------------------
TEST(EmitTest, StoreFrameImm_EverySize)
{
    Emitter e;
    e.StoreFrameImm(StoreAccessKind::ST_8, 1, 0); // st.frame.imm.1
    e.StoreFrameImm(StoreAccessKind::ST_16, 0x1234, 4); // st.frame.imm.2
    e.StoreFrameImm(StoreAccessKind::ST_32, 0x12345678, 8); // st.frame.imm.4
    e.StoreFrameImm(StoreAccessKind::ST_64, 0x1122334455667788ull, 12); // st.frame.imm.8
    e.StoreFrameImm(StoreAccessKind::ST_REF, 0, 16); // normalized to ST_64
    e.StoreFrameImm(StoreAccessKind::ST_F32, 0x1234, 20); // normalized to ST_32
    e.StoreFrameImm(StoreAccessKind::ST_F64, 0x12345678, 24); // normalized to ST_64

    auto code = e.Build(heap);
    CHECK_DISASM(
        code,
        "0x000: st.frame.imm.1 8 0x1 0x0",
        "0x007: st.frame.imm.2 16 0x1234 0x4",
        "0x00f: st.frame.imm.4 32 0x12345678 0x8",
        "0x019: st.frame.imm.8 64 0x1122334455667788 0xc",
        "0x027: st.frame.imm.1 64 0x0 0x10",
        "0x02e: st.frame.imm.2 32 0x1234 0x14",
        "0x036: st.frame.imm.4 64 0x12345678 0x18"
    );
}

} // namespace Emitter
} // namespace Cbc
