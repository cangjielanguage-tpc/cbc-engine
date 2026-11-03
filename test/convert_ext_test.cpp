#include "cbc_test_harness.h"

enum class ConvType { I8, U8, I16, U16, I32, U32, I64, U64, F32, F64 };

static const char* ConvTypeName(ConvType t)
{
    switch (t) {
        case ConvType::I8: return "I8";
        case ConvType::U8: return "U8";
        case ConvType::I16: return "I16";
        case ConvType::U16: return "U16";
        case ConvType::I32: return "I32";
        case ConvType::U32: return "U32";
        case ConvType::I64: return "I64";
        case ConvType::U64: return "U64";
        case ConvType::F32: return "F32";
        case ConvType::F64: return "F64";
    }
    return "";
}

static int ConvTypeWidth(ConvType t)
{
    switch (t) {
        case ConvType::I8:
        case ConvType::U8: return 8;
        case ConvType::I16:
        case ConvType::U16: return 16;
        case ConvType::I32:
        case ConvType::F32: return 32;
        default: return 64;
    }
}

struct ConvertExtResult {
    uint64_t value;
    bool overflow;
    Interpretation::Type error; // Specific cast-overflow reason; Successful when !overflow
};

// Mirrors CheckedCastOverflow in src/interpreter/casts.h.
template <typename To> struct TestCastOverflow;
template <> struct TestCastOverflow<int8_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowI8Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowI8Max;
};
template <> struct TestCastOverflow<uint8_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowU8Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowU8Max;
};
template <> struct TestCastOverflow<int16_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowI16Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowI16Max;
};
template <> struct TestCastOverflow<uint16_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowU16Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowU16Max;
};
template <> struct TestCastOverflow<int32_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowI32Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowI32Max;
};
template <> struct TestCastOverflow<uint32_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowU32Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowU32Max;
};
template <> struct TestCastOverflow<int64_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowI64Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowI64Max;
};
template <> struct TestCastOverflow<uint64_t> {
    static constexpr auto minExc = Interpretation::Type::CastOverflowU64Min;
    static constexpr auto maxExc = Interpretation::Type::CastOverflowU64Max;
};

template <typename To> static ConvertExtResult CheckedConvertOverflowed(bool minSide)
{
    return { 0, true, minSide ? TestCastOverflow<To>::minExc : TestCastOverflow<To>::maxExc };
}

// Mirrors CheckedCastFromSigned in src/interpreter/casts.h.
template <typename To> static ConvertExtResult CheckedConvertRefSigned(int64_t value)
{
    if constexpr (std::is_signed_v<To>) {
        if (value < static_cast<int64_t>(std::numeric_limits<To>::min())) {
            return CheckedConvertOverflowed<To>(true);
        }
        if (value > static_cast<int64_t>(std::numeric_limits<To>::max())) {
            return CheckedConvertOverflowed<To>(false);
        }
    } else {
        if (value < 0) {
            return CheckedConvertOverflowed<To>(true);
        }
        if constexpr (sizeof(To) < sizeof(uint64_t)) {
            if (static_cast<uint64_t>(value) > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
                return CheckedConvertOverflowed<To>(false);
            }
        }
    }
    return { static_cast<uint64_t>(static_cast<To>(value)), false };
}

// Mirrors CheckedCastFromUnsigned in src/interpreter/casts.h.
template <typename To> static ConvertExtResult CheckedConvertRefUnsigned(uint64_t value)
{
    if constexpr (std::is_signed_v<To>) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return CheckedConvertOverflowed<To>(false);
        }
    } else if constexpr (sizeof(To) < sizeof(uint64_t)) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return CheckedConvertOverflowed<To>(false);
        }
    }
    return { static_cast<uint64_t>(static_cast<To>(value)), false };
}

// Mirrors CheckedCastFromFp in src/interpreter/casts.h: truncation toward
// zero, representable iff the truncated value lies in [lower, upper).
template <typename To> static ConvertExtResult CheckedConvertRefFp(double value)
{
    if (!std::isfinite(value)) {
        return { 0, true, Interpretation::Type::CastOverflowInfOrNaN };
    }
    constexpr double upper = static_cast<double>(1ULL << (std::numeric_limits<To>::digits - 1)) * 2.0;
    constexpr double lower = std::is_signed_v<To> ? -upper : 0.0;
    const double truncated = std::trunc(value);
    if (truncated < lower) {
        return CheckedConvertOverflowed<To>(true);
    }
    if (truncated >= upper) {
        return CheckedConvertOverflowed<To>(false);
    }
    return { static_cast<uint64_t>(static_cast<To>(truncated)), false };
}

template <typename To> static ConvertExtResult CheckedConvertRefDst(ConvType src, uint64_t input)
{
    switch (src) {
        case ConvType::I8: return CheckedConvertRefSigned<To>(static_cast<int8_t>(input));
        case ConvType::I16: return CheckedConvertRefSigned<To>(static_cast<int16_t>(input));
        case ConvType::I32: return CheckedConvertRefSigned<To>(static_cast<int32_t>(static_cast<uint32_t>(input)));
        case ConvType::I64: return CheckedConvertRefSigned<To>(static_cast<int64_t>(input));
        case ConvType::U8: return CheckedConvertRefUnsigned<To>(static_cast<uint8_t>(input));
        case ConvType::U16: return CheckedConvertRefUnsigned<To>(static_cast<uint16_t>(input));
        case ConvType::U32: return CheckedConvertRefUnsigned<To>(static_cast<uint32_t>(input));
        default: return CheckedConvertRefUnsigned<To>(input);
    }
}

static ConvertExtResult CheckedConvertRef(ConvType dst, ConvType src, uint64_t input)
{
    switch (dst) {
        case ConvType::I8: return CheckedConvertRefDst<int8_t>(src, input);
        case ConvType::U8: return CheckedConvertRefDst<uint8_t>(src, input);
        case ConvType::I16: return CheckedConvertRefDst<int16_t>(src, input);
        case ConvType::U16: return CheckedConvertRefDst<uint16_t>(src, input);
        case ConvType::I32: return CheckedConvertRefDst<int32_t>(src, input);
        case ConvType::U32: return CheckedConvertRefDst<uint32_t>(src, input);
        case ConvType::I64: return CheckedConvertRefDst<int64_t>(src, input);
        default: return CheckedConvertRefDst<uint64_t>(src, input);
    }
}

static ConvertExtResult CheckedConvertRefFpSrc(ConvType dst, double input)
{
    switch (dst) {
        case ConvType::I8: return CheckedConvertRefFp<int8_t>(input);
        case ConvType::U8: return CheckedConvertRefFp<uint8_t>(input);
        case ConvType::I16: return CheckedConvertRefFp<int16_t>(input);
        case ConvType::U16: return CheckedConvertRefFp<uint16_t>(input);
        case ConvType::I32: return CheckedConvertRefFp<int32_t>(input);
        case ConvType::U32: return CheckedConvertRefFp<uint32_t>(input);
        case ConvType::I64: return CheckedConvertRefFp<int64_t>(input);
        default: return CheckedConvertRefFp<uint64_t>(input);
    }
}

// Mirrors CastInt in src/interpreter/casts.h for the i2i.u cases: the source
// is never narrower than the destination, so the result is a truncation
// zero-extended into the primitive.
static uint64_t I2iURef(ConvType dst, uint64_t input)
{
    switch (dst) {
        case ConvType::U8: return static_cast<uint8_t>(input);
        case ConvType::U16: return static_cast<uint16_t>(input);
        case ConvType::U32: return static_cast<uint32_t>(input);
        default: return input;
    }
}

// The same value shapes as the cast suite tables: integer boundaries of every
// width, values just inside and outside them, infinities and NaN.
static std::vector<uint64_t> ConvertExtIntInputs(int width)
{
    switch (width) {
        case 8: return { 0x0, 0x1, 0x2, 0x7F, 0x80, 0xFF, 0x180, 0x1FF };
        case 16: return { 0x0, 0x1, 0x2, 0x7FFF, 0x8000, 0xFFFF, 0x18000, 0x1FFFF };
        case 32: return { 0x0, 0x1, 0x2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x180000000, 0x1FFFFFFFF };
        default: return { 0x0, 0x1, 0x2, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF };
    }
}

static std::vector<double> ConvertExtFpInputs()
{
    return {
        0.0,
        -0.0,
        1.0,
        -1.0,
        1.5,
        -1.5,
        42.75,
        -42.75,
        127.9,
        -128.9,
        32767.9,
        -32768.9,
        2147483520.0,
        -2147483648.0,
        0x1p31,
        -0x1p31,
        0x1p63,
        -0x1p63,
        0x1p64,
        -0x1p64,
        0x1.fffffep30,
        0x1.fffffffffffffp62,
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
    };
}

struct ConvertExtParams {
    std::string name;
    ConvType dst;
    ConvType src;
    bool checked;
};

static std::ostream& operator<<(std::ostream& os, const ConvertExtParams& p) { return os << p.name; }

// The case set must stay in sync with generate_convert_ext in
// test/asm-generator/asm-generator.py.
static std::vector<ConvertExtParams> ConvertExtParamsList()
{
    const std::vector<ConvType> intTypes = { ConvType::I8,  ConvType::U8,  ConvType::I16, ConvType::U16,
                                             ConvType::I32, ConvType::U32, ConvType::I64, ConvType::U64 };
    std::vector<ConvertExtParams> list;
    auto add = [&](ConvType dst, ConvType src, bool checked) {
        list.push_back({ std::string(ConvTypeName(dst)) + "_" + ConvTypeName(src), dst, src, checked });
    };
    add(ConvType::U8, ConvType::I32, false);
    add(ConvType::U8, ConvType::U32, false);
    add(ConvType::U16, ConvType::I32, false);
    add(ConvType::U16, ConvType::U32, false);
    add(ConvType::U32, ConvType::U64, false);
    for (auto dst : intTypes) {
        for (auto src : intTypes) {
            add(dst, src, true);
        }
    }
    for (auto dst : intTypes) {
        for (auto src : { ConvType::F32, ConvType::F64 }) {
            add(dst, src, true);
        }
    }
    return list;
}

class CbcSpecializedConvertExt : public ::testing::TestWithParam<ConvertExtParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedConvertExt, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    ConvertExtParams params = GetParam();

    auto casePath = "./generated-asm/convert-ext/" + std::string(params.checked ? "checked" : "i2i-u") + "/" +
                    params.name + ".asm";
    auto code = OpenAndRewrite("test_" + params.name, casePath)->code;

    if (params.src == ConvType::F32 || params.src == ConvType::F64) {
        for (double v : ConvertExtFpInputs()) {
            // The interpreter widens an F32 source to double before the range
            // check; the reference must see the same rounded value.
            double input = params.src == ConvType::F32 ? static_cast<double>(static_cast<float>(v)) : v;
            auto seed    = params.src == ConvType::F32 ? F32(static_cast<float>(input)) : F64(input);
            auto ref     = CheckedConvertRefFpSrc(params.dst, input);
            SCOPED_TRACE(casePath + " input=" + hex(F64(input).u64));
            if (ref.overflow) {
                Interpret(code, U64(0), U64(0), seed, F64(0));
                EXPECT_EQ(Interpretation::LastImplicitThrow(), ref.error);
            } else {
                auto res = Interpret(code, U64(0), U64(0), seed, F64(0));
                EXPECT_EQ(res.u64, ref.value);
            }
        }
        return;
    }
    for (uint64_t v : ConvertExtIntInputs(ConvTypeWidth(params.src))) {
        auto ref = params.checked ? CheckedConvertRef(params.dst, params.src, v)
                                  : ConvertExtResult { I2iURef(params.dst, v), false };
        SCOPED_TRACE(casePath + " input=" + hex(v));
        if (ref.overflow) {
            EXPECT_EQ(InterpretThrows(code, U64(v), U64(0)), ref.error);
        } else {
            auto res = Interpret(code, U64(v), U64(0), F64(0), F64(0));
            EXPECT_EQ(res.u64, ref.value);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedConvertExt, ::testing::ValuesIn(ConvertExtParamsList()));
