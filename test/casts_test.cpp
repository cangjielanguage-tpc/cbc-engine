#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "interpreter/casts.h"

namespace {

using Interpretation::CastOp;
using Interpretation::CastPrim;
using Interpretation::CheckedCastOp;
using Interpretation::CheckedCastPrim;
using CastType = Cbc::Format::CastType;
using Prim = Interpretation::Value::Primitive;

constexpr uint64_t ALL_ONES = 0xffffffffffffffffULL;

Prim I8(int8_t v)
{
    Prim p {};
    p.u64 = static_cast<uint64_t>(v);
    return p;
}

Prim U8(uint8_t v)
{
    Prim p {};
    p.u64 = v;
    return p;
}

Prim I16(int16_t v)
{
    Prim p {};
    p.u64 = static_cast<uint64_t>(v);
    return p;
}

Prim U16(uint16_t v)
{
    Prim p {};
    p.u64 = v;
    return p;
}

Prim I32(int32_t v)
{
    Prim p {};
    p.u32 = static_cast<uint32_t>(v);
    return p;
}

Prim U32(uint32_t v)
{
    Prim p {};
    p.u32 = v;
    return p;
}

Prim I64(int64_t v)
{
    Prim p {};
    p.u64 = static_cast<uint64_t>(v);
    return p;
}

Prim U64(uint64_t v)
{
    Prim p {};
    p.u64 = v;
    return p;
}

Prim F32(float v)
{
    Prim p {};
    p.f32 = v;
    return p;
}

Prim F64(double v)
{
    Prim p {};
    p.f64 = v;
    return p;
}

template <typename T> std::string OpName(T op)
{
    return std::string(op.FromTypeStr()) + "->" + std::string(op.ToTypeStr());
}

void ExpectUncheckedInt(CastOp op, Prim in, uint64_t expected)
{
    SCOPED_TRACE(OpName(op));
    EXPECT_EQ(CastPrim(op, in).u64, expected);
}

void ExpectCheckedInt(CheckedCastOp op, Prim in, uint64_t expected, bool overflow)
{
    SCOPED_TRACE(OpName(op));
    auto res = CheckedCastPrim(op, in);
    EXPECT_EQ(res.overflow, overflow);
    if (!overflow) {
        EXPECT_EQ(res.value.u64, expected);
    }
}

struct UncheckedIntCase {
    CastOp op;
    Prim input;
    uint64_t expected;
};

struct CheckedIntCase {
    CheckedCastOp op;
    Prim input;
    uint64_t expected;
    bool overflow;
};

} // namespace

TEST(CastsTest, UncheckedInteger)
{
    const UncheckedIntCase cases[] = {
        { CastOp::Make(CastType::I32, CastType::I8), I32(0x12345678), 0x78 },
        { CastOp::Make(CastType::I32, CastType::I8), I32(-1), ALL_ONES },
        { CastOp::Make(CastType::I32, CastType::U8), I32(-1), 0xff },
        { CastOp::Make(CastType::I32, CastType::U8), I32(0x12345678), 0x78 },
        { CastOp::Make(CastType::I32, CastType::I16), I32(0x12345678), 0x5678 },
        { CastOp::Make(CastType::I32, CastType::I16), I32(-1), ALL_ONES },
        { CastOp::Make(CastType::I32, CastType::U16), I32(-1), 0xffff },
        { CastOp::Make(CastType::I32, CastType::U16), I32(0x12345678), 0x5678 },
        { CastOp::Make(CastType::I32, CastType::I64), I32(-1), ALL_ONES },
        { CastOp::Make(CastType::I32, CastType::I64), I32(0x12345678), 0x12345678 },
        { CastOp::Make(CastType::U32, CastType::I8), U32(0xffffff80), 0xffffffffffffff80ULL },
        { CastOp::Make(CastType::U32, CastType::U8), U32(0x12345678), 0x78 },
        { CastOp::Make(CastType::U32, CastType::I16), U32(0xffff8000), 0xffffffffffff8000ULL },
        { CastOp::Make(CastType::U32, CastType::U16), U32(0x12345678), 0x5678 },
        { CastOp::Make(CastType::U32, CastType::I64), U32(0xffffffff), 0xffffffff },
        { CastOp::Make(CastType::I64, CastType::I32), I64(0x123456789abcdef0LL), 0xffffffff9abcdef0ULL },
        { CastOp::Make(CastType::U64, CastType::I32), U64(0xffffffffffffffffULL), ALL_ONES },
        { CastOp::Make(CastType::U64, CastType::U32), U64(0x123456789abcdef0ULL), 0x9abcdef0 },
    };

    for (const auto& c : cases) {
        ExpectUncheckedInt(c.op, c.input, c.expected);
    }
}

TEST(CastsTest, UncheckedIntToFloat)
{
    struct F32Case {
        CastOp op;
        Prim input;
        float expected;
    };

    const F32Case f32Cases[] = {
        { CastOp::Make(CastType::I32, CastType::F32), I32(-42), -42.0f },
        { CastOp::Make(CastType::U32, CastType::F32), U32(42), 42.0f },
        { CastOp::Make(CastType::I64, CastType::F32), I64(-42), -42.0f },
        { CastOp::Make(CastType::U64, CastType::F32), U64(42), 42.0f },
    };
    for (const auto& c : f32Cases) {
        SCOPED_TRACE(OpName(c.op));
        EXPECT_FLOAT_EQ(CastPrim(c.op, c.input).f32, c.expected);
    }

    struct F64Case {
        CastOp op;
        Prim input;
        double expected;
    };

    const F64Case f64Cases[] = {
        { CastOp::Make(CastType::I32, CastType::F64), I32(-42), -42.0 },
        { CastOp::Make(CastType::U32, CastType::F64), U32(42), 42.0 },
        { CastOp::Make(CastType::I64, CastType::F64), I64(-42), -42.0 },
        { CastOp::Make(CastType::U64, CastType::F64), U64(42), 42.0 },
    };
    for (const auto& c : f64Cases) {
        SCOPED_TRACE(OpName(c.op));
        EXPECT_DOUBLE_EQ(CastPrim(c.op, c.input).f64, c.expected);
    }
}

TEST(CastsTest, UncheckedFloatToInt)
{
    const float nan32  = std::numeric_limits<float>::quiet_NaN();
    const double nan64 = std::numeric_limits<double>::quiet_NaN();

    const UncheckedIntCase cases[] = {
        { CastOp::Make(CastType::F32, CastType::I8), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::I8), F32(nan32), 0 },
        { CastOp::Make(CastType::F32, CastType::I8), F32(300.0f), 127 },
        { CastOp::Make(CastType::F32, CastType::I8), F32(-300.0f), 0xffffffffffffff80ULL },
        { CastOp::Make(CastType::F32, CastType::U8), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::U8), F32(-42.9f), 0 },
        { CastOp::Make(CastType::F32, CastType::U8), F32(300.0f), 255 },
        { CastOp::Make(CastType::F32, CastType::I16), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::I16), F32(40000.0f), 32767 },
        { CastOp::Make(CastType::F32, CastType::I16), F32(-40000.0f), 0xffffffffffff8000ULL },
        { CastOp::Make(CastType::F32, CastType::U16), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::U16), F32(-1.0f), 0 },
        { CastOp::Make(CastType::F32, CastType::U16), F32(70000.0f), 65535 },
        { CastOp::Make(CastType::F32, CastType::I32), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::I32), F32(3e9f), 2147483647 },
        { CastOp::Make(CastType::F32, CastType::U32), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::U32), F32(-1.0f), 0 },
        { CastOp::Make(CastType::F32, CastType::U32), F32(5e9f), 4294967295 },
        { CastOp::Make(CastType::F32, CastType::I64), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::I64), F32(-42.9f), 0xffffffffffffffd6ULL },
        { CastOp::Make(CastType::F32, CastType::U64), F32(42.9f), 42 },
        { CastOp::Make(CastType::F32, CastType::U64), F32(-1.0f), 0 },

        { CastOp::Make(CastType::F64, CastType::I8), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::I8), F64(nan64), 0 },
        { CastOp::Make(CastType::F64, CastType::I8), F64(300.0), 127 },
        { CastOp::Make(CastType::F64, CastType::I8), F64(-300.0), 0xffffffffffffff80ULL },
        { CastOp::Make(CastType::F64, CastType::U8), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::U8), F64(-1.0), 0 },
        { CastOp::Make(CastType::F64, CastType::U8), F64(300.0), 255 },
        { CastOp::Make(CastType::F64, CastType::I16), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::I16), F64(40000.0), 32767 },
        { CastOp::Make(CastType::F64, CastType::I16), F64(-40000.0), 0xffffffffffff8000ULL },
        { CastOp::Make(CastType::F64, CastType::U16), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::U16), F64(-1.0), 0 },
        { CastOp::Make(CastType::F64, CastType::U16), F64(70000.0), 65535 },
        { CastOp::Make(CastType::F64, CastType::I32), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::I32), F64(3e9), 2147483647 },
        { CastOp::Make(CastType::F64, CastType::I32), F64(-3e9), 0xffffffff80000000ULL },
        { CastOp::Make(CastType::F64, CastType::U32), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::U32), F64(-1.0), 0 },
        { CastOp::Make(CastType::F64, CastType::U32), F64(5e9), 4294967295 },
        { CastOp::Make(CastType::F64, CastType::I64), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::I64), F64(-42.9), 0xffffffffffffffd6ULL },
        { CastOp::Make(CastType::F64, CastType::U64), F64(42.9), 42 },
        { CastOp::Make(CastType::F64, CastType::U64), F64(-1.0), 0 },
    };

    for (const auto& c : cases) {
        ExpectUncheckedInt(c.op, c.input, c.expected);
    }
}

TEST(CastsTest, UncheckedFloatToFloat)
{
    EXPECT_DOUBLE_EQ(CastPrim(CastOp::Make(CastType::F32, CastType::F64), F32(1.5f)).f64, 1.5);
    EXPECT_FLOAT_EQ(CastPrim(CastOp::Make(CastType::F64, CastType::F32), F64(1.5)).f32, 1.5f);
}

TEST(CastsTest, CheckedInteger)
{
    const CheckedIntCase cases[] = {
        { CheckedCastOp::Make(CastType::I8, CastType::U8), I8(127), 127, false },
        { CheckedCastOp::Make(CastType::I8, CastType::U8), I8(-1), 0, true },
        { CheckedCastOp::Make(CastType::U8, CastType::I8), U8(127), 127, false },
        { CheckedCastOp::Make(CastType::U8, CastType::I8), U8(128), 0, true },

        { CheckedCastOp::Make(CastType::I16, CastType::I8), I16(127), 127, false },
        { CheckedCastOp::Make(CastType::I16, CastType::I8), I16(128), 0, true },
        { CheckedCastOp::Make(CastType::I16, CastType::I8), I16(-128), 0xffffffffffffff80ULL, false },
        { CheckedCastOp::Make(CastType::I16, CastType::I8), I16(-129), 0, true },
        { CheckedCastOp::Make(CastType::I16, CastType::U8), I16(255), 255, false },
        { CheckedCastOp::Make(CastType::I16, CastType::U8), I16(256), 0, true },
        { CheckedCastOp::Make(CastType::I16, CastType::U8), I16(-1), 0, true },
        { CheckedCastOp::Make(CastType::I16, CastType::U16), I16(32767), 32767, false },
        { CheckedCastOp::Make(CastType::I16, CastType::U16), I16(-1), 0, true },
        { CheckedCastOp::Make(CastType::U16, CastType::I8), U16(127), 127, false },
        { CheckedCastOp::Make(CastType::U16, CastType::I8), U16(128), 0, true },
        { CheckedCastOp::Make(CastType::U16, CastType::U8), U16(255), 255, false },
        { CheckedCastOp::Make(CastType::U16, CastType::U8), U16(256), 0, true },
        { CheckedCastOp::Make(CastType::U16, CastType::I16), U16(32767), 32767, false },
        { CheckedCastOp::Make(CastType::U16, CastType::I16), U16(32768), 0, true },

        { CheckedCastOp::Make(CastType::I32, CastType::I8), I32(127), 127, false },
        { CheckedCastOp::Make(CastType::I32, CastType::I8), I32(128), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::I8), I32(-129), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U8), I32(255), 255, false },
        { CheckedCastOp::Make(CastType::I32, CastType::U8), I32(256), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U8), I32(-1), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::I16), I32(32767), 32767, false },
        { CheckedCastOp::Make(CastType::I32, CastType::I16), I32(32768), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::I16), I32(-32769), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U16), I32(65535), 65535, false },
        { CheckedCastOp::Make(CastType::I32, CastType::U16), I32(65536), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U16), I32(-1), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U32), I32(0), 0, false },
        { CheckedCastOp::Make(CastType::I32, CastType::U32), I32(-1), 0, true },
        { CheckedCastOp::Make(CastType::I32, CastType::U32), I32(0x7fffffff), 0x7fffffff, false },
        { CheckedCastOp::Make(CastType::U32, CastType::I8), U32(127), 127, false },
        { CheckedCastOp::Make(CastType::U32, CastType::I8), U32(128), 0, true },
        { CheckedCastOp::Make(CastType::U32, CastType::U8), U32(255), 255, false },
        { CheckedCastOp::Make(CastType::U32, CastType::U8), U32(256), 0, true },
        { CheckedCastOp::Make(CastType::U32, CastType::I16), U32(32767), 32767, false },
        { CheckedCastOp::Make(CastType::U32, CastType::I16), U32(32768), 0, true },
        { CheckedCastOp::Make(CastType::U32, CastType::U16), U32(65535), 65535, false },
        { CheckedCastOp::Make(CastType::U32, CastType::U16), U32(65536), 0, true },
        { CheckedCastOp::Make(CastType::U32, CastType::I32), U32(0x7fffffff), 0x7fffffff, false },
        { CheckedCastOp::Make(CastType::U32, CastType::I32), U32(0x80000000), 0, true },

        { CheckedCastOp::Make(CastType::I64, CastType::I8), I64(127), 127, false },
        { CheckedCastOp::Make(CastType::I64, CastType::I8), I64(128), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::I8), I64(-129), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U8), I64(255), 255, false },
        { CheckedCastOp::Make(CastType::I64, CastType::U8), I64(256), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U8), I64(-1), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::I16), I64(32767), 32767, false },
        { CheckedCastOp::Make(CastType::I64, CastType::I16), I64(32768), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::I16), I64(-32769), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U16), I64(65535), 65535, false },
        { CheckedCastOp::Make(CastType::I64, CastType::U16), I64(65536), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U16), I64(-1), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::I32), I64(0x7fffffff), 0x7fffffff, false },
        { CheckedCastOp::Make(CastType::I64, CastType::I32), I64(0x80000000), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::I32), I64(-2147483649LL), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U32), I64(0xffffffff), 0xffffffff, false },
        { CheckedCastOp::Make(CastType::I64, CastType::U32), I64(0x100000000LL), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U32), I64(-1), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U64), I64(0), 0, false },
        { CheckedCastOp::Make(CastType::I64, CastType::U64), I64(-1), 0, true },
        { CheckedCastOp::Make(CastType::I64, CastType::U64), I64(0x7fffffffffffffffLL), 0x7fffffffffffffffULL, false },
        { CheckedCastOp::Make(CastType::U64, CastType::I8), U64(127), 127, false },
        { CheckedCastOp::Make(CastType::U64, CastType::I8), U64(128), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::U8), U64(255), 255, false },
        { CheckedCastOp::Make(CastType::U64, CastType::U8), U64(256), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::I16), U64(32767), 32767, false },
        { CheckedCastOp::Make(CastType::U64, CastType::I16), U64(32768), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::U16), U64(65535), 65535, false },
        { CheckedCastOp::Make(CastType::U64, CastType::U16), U64(65536), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::I32), U64(0x7fffffff), 0x7fffffff, false },
        { CheckedCastOp::Make(CastType::U64, CastType::I32), U64(0x80000000), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::U32), U64(0xffffffff), 0xffffffff, false },
        { CheckedCastOp::Make(CastType::U64, CastType::U32), U64(0x100000000ULL), 0, true },
        { CheckedCastOp::Make(CastType::U64, CastType::I64), U64(0x7fffffffffffffffULL), 0x7fffffffffffffffULL, false },
        { CheckedCastOp::Make(CastType::U64, CastType::I64), U64(0x8000000000000000ULL), 0, true },
    };

    for (const auto& c : cases) {
        ExpectCheckedInt(c.op, c.input, c.expected, c.overflow);
    }
}

TEST(CastsTest, CheckedFloatToInt)
{
    const float nan32  = std::numeric_limits<float>::quiet_NaN();
    const float inf32  = std::numeric_limits<float>::infinity();
    const double nan64 = std::numeric_limits<double>::quiet_NaN();
    const double inf64 = std::numeric_limits<double>::infinity();

    const CheckedIntCase cases[] = {
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(127.9f), 127, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(128.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(-128.9f), 0xffffffffffffff80ULL, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(-129.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(nan32), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I8), F32(inf32), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(255.9f), 255, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(256.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(-0.9f), 0, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(-1.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U8), F32(nan32), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I16), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I16), F32(32768.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I16), F32(-32769.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U16), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U16), F32(65536.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U16), F32(-1.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I32), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I32), F32(2147483648.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I32), F32(-3e9f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U32), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U32), F32(4294967296.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U32), F32(-1.0f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I64), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::I64), F32(1e30f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::I64), F32(nan32), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U64), F32(42.9f), 42, false },
        { CheckedCastOp::Make(CastType::F32, CastType::U64), F32(1e30f), 0, true },
        { CheckedCastOp::Make(CastType::F32, CastType::U64), F32(-1.0f), 0, true },

        { CheckedCastOp::Make(CastType::F64, CastType::I8), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::I8), F64(128.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I8), F64(-129.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I8), F64(nan64), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U8), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::U8), F64(256.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U8), F64(-1.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I16), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::I16), F64(32768.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I16), F64(-32769.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U16), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::U16), F64(65536.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U16), F64(-1.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I32), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::I32), F64(2147483648.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I32), F64(-2147483649.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U32), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::U32), F64(4294967296.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U32), F64(-1.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I64), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::I64), F64(9223372036854775808.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::I64), F64(-1e30), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U64), F64(42.9), 42, false },
        { CheckedCastOp::Make(CastType::F64, CastType::U64), F64(18446744073709551616.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U64), F64(-1.0), 0, true },
        { CheckedCastOp::Make(CastType::F64, CastType::U64), F64(inf64), 0, true },
    };

    for (const auto& c : cases) {
        ExpectCheckedInt(c.op, c.input, c.expected, c.overflow);
    }
}

TEST(CastsTest, OpcodeDispatch)
{
    EXPECT_EQ(CastPrim(CastOp::Make(CastType::U32, CastType::U8), U32(0x100)).u64, 0);

    auto checkedInRange = CheckedCastPrim(CheckedCastOp::Make(CastType::U32, CastType::U8), U32(0xff));
    EXPECT_FALSE(checkedInRange.overflow);
    EXPECT_EQ(checkedInRange.value.u64, 0xff);

    auto checkedOverflow = CheckedCastPrim(CheckedCastOp::Make(CastType::U32, CastType::U8), U32(0x100));
    EXPECT_TRUE(checkedOverflow.overflow);
}

TEST(CastsTest, OpPairs)
{
    EXPECT_FALSE(CastOp::FromPair(CastType::I32, CastType::U32).has_value());
    EXPECT_TRUE(CastOp::FromPair(CastType::I32, CastType::I64).has_value());
    EXPECT_TRUE(CastOp::FromPair(CastType::F64, CastType::F32).has_value());
    EXPECT_TRUE(CheckedCastOp::FromPair(CastType::I32, CastType::U8).has_value());
    EXPECT_FALSE(CheckedCastOp::FromPair(CastType::I32, CastType::I64).has_value());
    EXPECT_FALSE(CheckedCastOp::FromPair(CastType::F64, CastType::F32).has_value());

    EXPECT_FALSE(CastOp::FromPair(CastType::COUNT, CastType::I8).has_value());
}

TEST(CastsTest, OpCoverage)
{
    const Prim zero = U64(0);
    for (size_t i = 0; i < CastOp::COUNT; i++) {
        (void)CastPrim(CastOp::Make(CastOp::PairAt(i).from, CastOp::PairAt(i).to), zero);
    }
    for (size_t i = 0; i < CheckedCastOp::COUNT; i++) {
        auto op = CheckedCastOp::Make(CheckedCastOp::PairAt(i).from, CheckedCastOp::PairAt(i).to);
        EXPECT_FALSE(CheckedCastPrim(op, zero).overflow) << "checked op " << i << " " << OpName(op);
    }
}
