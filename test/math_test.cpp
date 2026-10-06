#include "utils/math.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

std::string HexBitsName(char const* prefix, uint64_t value, uint32_t bits)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%s%llXb%u", prefix, static_cast<unsigned long long>(value), bits);
    return buf;
}

struct BitsCase {
    uint64_t value;
    uint32_t bits;
    uint64_t expected;
};

std::string BitsCaseName(const testing::TestParamInfo<BitsCase>& info)
{
    return HexBitsName("v", info.param.value, info.param.bits);
}

struct SignedBitsCase {
    int64_t value;
    uint32_t bits;
    bool expected;
};

std::string SignedBitsCaseName(const testing::TestParamInfo<SignedBitsCase>& info)
{
    char buf[48];
    uint64_t magnitude =
        info.param.value < 0 ? 0ull - static_cast<uint64_t>(info.param.value) : static_cast<uint64_t>(info.param.value);
    snprintf(
        buf,
        sizeof(buf),
        "%s%llub%u",
        info.param.value < 0 ? "neg" : "v",
        static_cast<unsigned long long>(magnitude),
        info.param.bits
    );
    return buf;
}

struct UnsignedBitsCase {
    uint64_t value;
    uint32_t bits;
    bool expected;
};

std::string UnsignedBitsCaseName(const testing::TestParamInfo<UnsignedBitsCase>& info)
{
    return HexBitsName("v", info.param.value, info.param.bits);
}

struct WidthCase {
    uint32_t bits;
    uint64_t expected;
};

std::string WidthCaseName(const testing::TestParamInfo<WidthCase>& info)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "n%u", info.param.bits);
    return buf;
}

} // namespace

TEST(MathUtils, AlignUp)
{
    ASSERT_EQ(0x0, MathUtils::AlignUp(0x0, 4));
    ASSERT_EQ(0x4, MathUtils::AlignUp(0x1, 4));
    ASSERT_EQ(0x4, MathUtils::AlignUp(0x4, 4));
    ASSERT_EQ(0x8, MathUtils::AlignUp(0x5, 4));
    ASSERT_EQ(0x10, MathUtils::AlignUp(0xF, 16));
}

class MathUtilsIsNBits : public testing::TestWithParam<std::tuple<uint64_t, uint32_t, uint32_t>> {};

TEST_P(MathUtilsIsNBits, ValueFitsInBits)
{
    auto [value, minWidth, bits] = GetParam();
    EXPECT_EQ(bits >= minWidth, MathUtils::IsNBits(value, bits)) << "value=" << value << " bits=" << bits;
}

INSTANTIATE_TEST_SUITE_P(
    Sentinels,
    MathUtilsIsNBits,
    testing::ValuesIn([] {
        std::vector<std::tuple<uint64_t, uint32_t, uint32_t>> cases;
        const std::pair<uint64_t, uint32_t> sentinels[] = {
            { 0x0L, 0 },
            { 0xFL, 4 },
            { 0xFFFL, 12 },
            { 0xFFFFFFFFL, 32 },
            { 0xFFFFFFFFFFFFL, 48 },
            { 0xFFFFFFFFFFFFFFFFL, 64 },
        };
        for (auto [value, minWidth] : sentinels) {
            for (uint32_t bits = 0; bits <= 64; ++bits) {
                cases.emplace_back(value, minWidth, bits);
            }
        }
        return cases;
    }()),
    [](const testing::TestParamInfo<MathUtilsIsNBits::ParamType>& info) {
        return HexBitsName("v", std::get<0>(info.param), std::get<2>(info.param));
    }
);

class MathUtilsIsNBitsSignedI32 : public testing::TestWithParam<SignedBitsCase> {};

TEST_P(MathUtilsIsNBitsSignedI32, ValueFitsInSignedBits)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::IsNBitsSigned<int32_t>(static_cast<int32_t>(c.value), c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    MathUtilsIsNBitsSignedI32,
    testing::Values(
        SignedBitsCase { 0x7, 4, true },
        SignedBitsCase { 0x8, 4, false },
        SignedBitsCase { 0xF, 2, false },
        SignedBitsCase { 0xF, 4, false },
        SignedBitsCase { 0xF, 5, true },
        SignedBitsCase { 0x7F, 8, true },
        SignedBitsCase { 0x80, 8, false },
        SignedBitsCase { 0xFF, 4, false },
        SignedBitsCase { 0xFF, 8, false },
        SignedBitsCase { 0xFF, 9, true },
        SignedBitsCase { 0x7FFFFFFF, 32, true },
        SignedBitsCase { 0x80000000, 32, true },
        SignedBitsCase { 0xFFFFFF37, 8, false },
        SignedBitsCase { 0xFFFFFF37, 9, true },
        SignedBitsCase { 0xFFFFFFFF, 2, true },
        SignedBitsCase { 0xFFFFFFFF, 8, true },
        SignedBitsCase { 0xFFFFFFFF, 32, true }
    ),
    SignedBitsCaseName
);

class MathUtilsIsNBitsSignedI64 : public testing::TestWithParam<SignedBitsCase> {};

TEST_P(MathUtilsIsNBitsSignedI64, ValueFitsInSignedBits)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::IsNBitsSigned<int64_t>(c.value, c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    MathUtilsIsNBitsSignedI64,
    testing::Values(
        SignedBitsCase { 0x7, 4, true },
        SignedBitsCase { 0x8, 4, false },
        SignedBitsCase { 0xF, 2, false },
        SignedBitsCase { 0xF, 4, false },
        SignedBitsCase { 0xF, 5, true },
        SignedBitsCase { 0x7F, 8, true },
        SignedBitsCase { 0x80, 8, false },
        SignedBitsCase { 0xFF, 4, false },
        SignedBitsCase { 0xFF, 8, false },
        SignedBitsCase { 0xFF, 9, true },
        SignedBitsCase { INT64_MAX, 64, true },
        SignedBitsCase { INT64_MIN, 63, false },
        SignedBitsCase { INT64_MIN, 64, true },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFF37ull), 8, false },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFF37ull), 9, true },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFFFFull), 2, true },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFFFFull), 8, true },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFFFFull), 32, true },
        SignedBitsCase { static_cast<int64_t>(0xFFFFFFFFFFFFFFFFull), 64, true }
    ),
    SignedBitsCaseName
);

class MathUtilsIsNBitsSignedU32 : public testing::TestWithParam<UnsignedBitsCase> {};

TEST_P(MathUtilsIsNBitsSignedU32, ValueReinterpretedAsSignedFitsInBits)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::IsNBitsSigned(static_cast<uint32_t>(c.value), c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    MathUtilsIsNBitsSignedU32,
    testing::Values(
        UnsignedBitsCase { 0x7, 4, true },
        UnsignedBitsCase { 0xF, 4, false },
        UnsignedBitsCase { 0xF, 5, true },
        UnsignedBitsCase { 0x7F, 8, true },
        UnsignedBitsCase { 0x80, 8, false },
        UnsignedBitsCase { 0xFF, 8, false },
        UnsignedBitsCase { 0xFF, 9, true },
        UnsignedBitsCase { 0x7FFFFFFF, 32, true },
        UnsignedBitsCase { 0x80000000, 31, false },
        UnsignedBitsCase { 0x80000000, 32, true },
        UnsignedBitsCase { 0xFFFFFF37, 8, false },
        UnsignedBitsCase { 0xFFFFFF37, 9, true },
        UnsignedBitsCase { 0xFFFFFFFF, 2, true },
        UnsignedBitsCase { 0xFFFFFFFF, 32, true }
    ),
    UnsignedBitsCaseName
);

class MathUtilsIsNBitsSignedU64 : public testing::TestWithParam<UnsignedBitsCase> {};

TEST_P(MathUtilsIsNBitsSignedU64, ValueReinterpretedAsSignedFitsInBits)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::IsNBitsSigned(c.value, c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    MathUtilsIsNBitsSignedU64,
    testing::Values(
        UnsignedBitsCase { 0x7F, 8, true },
        UnsignedBitsCase { 0x80, 8, false },
        UnsignedBitsCase { 0x7FFFFFFFFFFFFFFFull, 64, true },
        UnsignedBitsCase { 0x8000000000000000ull, 63, false },
        UnsignedBitsCase { 0x8000000000000000ull, 64, true },
        UnsignedBitsCase { 0xFFFFFFFFFFFFFF37ull, 8, false },
        UnsignedBitsCase { 0xFFFFFFFFFFFFFF37ull, 9, true },
        UnsignedBitsCase { 0xFFFFFFFFFFFFFFFFull, 64, true }
    ),
    UnsignedBitsCaseName
);

class MathUtilsRightNBits32 : public testing::TestWithParam<WidthCase> {};

TEST_P(MathUtilsRightNBits32, MaskOfWidth)
{
    EXPECT_EQ(static_cast<uint32_t>(GetParam().expected), MathUtils::RightNBits32(GetParam().bits));
}

INSTANTIATE_TEST_SUITE_P(
    Widths,
    MathUtilsRightNBits32,
    testing::Values(
        WidthCase { 0, 0x0 },
        WidthCase { 1, 0x1 },
        WidthCase { 3, 0x7 },
        WidthCase { 8, 0xFF },
        WidthCase { 12, 0xFFF },
        WidthCase { 16, 0xFFFF },
        WidthCase { 31, 0x7FFFFFFF },
        WidthCase { 32, 0xFFFFFFFF }
    ),
    WidthCaseName
);

class MathUtilsRightNBits64 : public testing::TestWithParam<WidthCase> {};

TEST_P(MathUtilsRightNBits64, MaskOfWidth) { EXPECT_EQ(GetParam().expected, MathUtils::RightNBits64(GetParam().bits)); }

INSTANTIATE_TEST_SUITE_P(
    Widths,
    MathUtilsRightNBits64,
    testing::Values(
        WidthCase { 0, 0x0 },
        WidthCase { 1, 0x1 },
        WidthCase { 3, 0x7 },
        WidthCase { 8, 0xFF },
        WidthCase { 12, 0xFFF },
        WidthCase { 16, 0xFFFF },
        WidthCase { 31, 0x7FFFFFFF },
        WidthCase { 32, 0xFFFFFFFF },
        WidthCase { 48, 0xFFFFFFFFFFFF },
        WidthCase { 63, 0x7FFFFFFFFFFFFFFF },
        WidthCase { 64, 0xFFFFFFFFFFFFFFFF }
    ),
    WidthCaseName
);

class MathUtilsSignExtend32 : public testing::TestWithParam<BitsCase> {};

TEST_P(MathUtilsSignExtend32, MatchesExpected)
{
    auto c = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(c.expected), MathUtils::SignExtend(static_cast<uint32_t>(c.value), c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    MathUtilsSignExtend32,
    testing::Values(
        BitsCase { 0x0, 1, 0x0 },
        BitsCase { 0x1, 1, 0xFFFFFFFF },
        BitsCase { 0x0, 2, 0x0 },
        BitsCase { 0x1, 2, 0x1 },
        BitsCase { 0x2, 2, 0xFFFFFFFE },
        BitsCase { 0x3, 2, 0xFFFFFFFF },
        BitsCase { 0x7, 4, 0x7 },
        BitsCase { 0x8, 4, 0xFFFFFFF8 },
        BitsCase { 0x9, 4, 0xFFFFFFF9 },
        BitsCase { 0x7F, 8, 0x7F },
        BitsCase { 0x80, 8, 0xFFFFFF80 },
        BitsCase { 0x9, 14, 0x9 },
        BitsCase { 0x0, 28, 0x0 },
        BitsCase { 0x7FFF, 16, 0x7FFF },
        BitsCase { 0x8000, 16, 0xFFFF8000 },
        BitsCase { 0xFFFF, 16, 0xFFFFFFFF },
        BitsCase { 0x0, 32, 0x0 },
        BitsCase { 0x7FFFFFFF, 32, 0x7FFFFFFF },
        BitsCase { 0xFFFFFFFF, 32, 0xFFFFFFFF }
    ),
    BitsCaseName
);

class MathUtilsSignExtend64 : public testing::TestWithParam<BitsCase> {};

TEST_P(MathUtilsSignExtend64, MatchesExpected)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::SignExtend(c.value, c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    MathUtilsSignExtend64,
    testing::Values(
        BitsCase { 0x0, 1, 0x0 },
        BitsCase { 0x1, 1, 0xFFFFFFFFFFFFFFFF },
        BitsCase { 0x0, 2, 0x0 },
        BitsCase { 0x1, 2, 0x1 },
        BitsCase { 0x2, 2, 0xFFFFFFFFFFFFFFFE },
        BitsCase { 0x3, 2, 0xFFFFFFFFFFFFFFFF },
        BitsCase { 0x7F, 8, 0x7F },
        BitsCase { 0x80, 8, 0xFFFFFFFFFFFFFF80 },
        BitsCase { 0x9, 4, 0xFFFFFFFFFFFFFFF9 },
        BitsCase { 0x9, 14, 0x9 },
        BitsCase { 0x0, 63, 0x0 },
        BitsCase { 0x7FFF, 16, 0x7FFF },
        BitsCase { 0x8000, 16, 0xFFFFFFFFFFFF8000 },
        BitsCase { 0xFFFF, 16, 0xFFFFFFFFFFFFFFFF },
        BitsCase { 0x0, 64, 0x0 },
        BitsCase { 0x7FFFFFFFFFFFFFFF, 64, 0x7FFFFFFFFFFFFFFF },
        BitsCase { 0xFFFFFFFFFFFFFFFF, 64, 0xFFFFFFFFFFFFFFFF }
    ),
    BitsCaseName
);

class MathUtilsZeroExtend32 : public testing::TestWithParam<BitsCase> {};

TEST_P(MathUtilsZeroExtend32, MatchesExpected)
{
    auto c = GetParam();
    EXPECT_EQ(static_cast<uint32_t>(c.expected), MathUtils::ZeroExtend(static_cast<uint32_t>(c.value), c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    MathUtilsZeroExtend32,
    testing::Values(
        BitsCase { 0x0, 1, 0x0 },
        BitsCase { 0x1, 1, 0x1 },
        BitsCase { 0x0, 2, 0x0 },
        BitsCase { 0x1, 2, 0x1 },
        BitsCase { 0x3, 2, 0x3 },
        BitsCase { 0x9, 4, 0x9 },
        BitsCase { 0xFF, 8, 0xFF },
        BitsCase { 0x1FF, 8, 0xFF },
        BitsCase { 0x9, 14, 0x9 },
        BitsCase { 0x0, 28, 0x0 },
        BitsCase { 0xFFFF, 16, 0xFFFF },
        BitsCase { 0x1FFFF, 16, 0xFFFF },
        BitsCase { 0x0, 32, 0x0 },
        BitsCase { 0xFFFFFFFF, 32, 0xFFFFFFFF }
    ),
    BitsCaseName
);

class MathUtilsZeroExtend64 : public testing::TestWithParam<BitsCase> {};

TEST_P(MathUtilsZeroExtend64, MatchesExpected)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::ZeroExtend(c.value, c.bits));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    MathUtilsZeroExtend64,
    testing::Values(
        BitsCase { 0x0, 1, 0x0 },
        BitsCase { 0x1, 1, 0x1 },
        BitsCase { 0x0, 2, 0x0 },
        BitsCase { 0x1, 2, 0x1 },
        BitsCase { 0x9, 4, 0x9 },
        BitsCase { 0x9, 14, 0x9 },
        BitsCase { 0x0, 63, 0x0 },
        BitsCase { 0x1FFFF, 16, 0xFFFF },
        BitsCase { 0x0, 64, 0x0 },
        BitsCase { 0xFFFFFFFFFFFFFFFF, 63, 0x7FFFFFFFFFFFFFFF },
        BitsCase { 0xFFFFFFFFFFFFFFFF, 64, 0xFFFFFFFFFFFFFFFF }
    ),
    BitsCaseName
);

class MathUtilsAlignUpProperty : public testing::TestWithParam<uint32_t> {};

TEST_P(MathUtilsAlignUpProperty, AlignsToNextMultiple)
{
    const uint32_t alignment = GetParam();
    for (uint32_t value = 0; value <= 4096; ++value) {
        const uint32_t aligned = MathUtils::AlignUp(value, alignment);
        EXPECT_EQ(0u, aligned % alignment) << "value=" << value;
        EXPECT_GE(aligned, value) << "value=" << value;
        EXPECT_LT(aligned - value, alignment) << "value=" << value;
        EXPECT_EQ(aligned, MathUtils::AlignUp(aligned, alignment)) << "value=" << value;
    }
}

INSTANTIATE_TEST_SUITE_P(
    Alignments, MathUtilsAlignUpProperty, testing::Values(1u, 2u, 3u, 4u, 5u, 7u, 8u, 16u, 64u, 1024u)
);

class MathUtilsSignExtend32Property : public testing::TestWithParam<uint32_t> {};

TEST_P(MathUtilsSignExtend32Property, RoundTripAndSignedRange)
{
    const uint32_t bits      = GetParam();
    const uint32_t widthMask = (bits == 32) ? 0xFFFFFFFFu : MathUtils::RightNBits32(bits);
    const uint32_t signBit   = (bits == 32) ? 0x80000000u : (1u << (bits - 1));
    auto checkValue          = [&](uint32_t value) {
        const uint32_t extended = MathUtils::SignExtend(value, bits);
        EXPECT_EQ(value, MathUtils::ZeroExtend(extended, bits)) << "value=" << value;
        if (value & signBit) {
            EXPECT_GE(extended, widthMask - signBit + 1) << "value=" << value;
        } else {
            EXPECT_LT(extended, signBit) << "value=" << value;
        }
    };
    if (bits <= 12) {
        for (uint32_t value = 0; value <= widthMask; ++value) {
            checkValue(value);
        }
    } else {
        for (uint32_t value : { 0u,
                                1u,
                                signBit - 1u,
                                signBit,
                                signBit + 1u,
                                widthMask - 1u,
                                widthMask,
                                0xA5A5A5A5u & widthMask,
                                0xDEADBEEFu & widthMask }) {
            checkValue(value & widthMask);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(BitWidths, MathUtilsSignExtend32Property, testing::Range(1u, 33u));

struct BitsRangeCase {
    uint64_t value;
    uint32_t from;
    uint32_t to;
    uint64_t expected;
};

std::string BitsRangeCaseName(const testing::TestParamInfo<BitsRangeCase>& info)
{
    char buf[64];
    snprintf(
        buf,
        sizeof(buf),
        "v%llXf%ut%u",
        static_cast<unsigned long long>(info.param.value),
        info.param.from,
        info.param.to
    );
    return buf;
}

class MathUtilsBits : public testing::TestWithParam<BitsRangeCase> {};

TEST_P(MathUtilsBits, ExtractsInclusiveRange)
{
    auto c = GetParam();
    EXPECT_EQ(c.expected, MathUtils::Bits(c.value, c.from, c.to));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    MathUtilsBits,
    testing::Values(
        BitsRangeCase { 0x3EFL, 3, 5, 0x5 },
        BitsRangeCase { 0x2F0L, 7, 9, 0x5 },
        BitsRangeCase { 0x3FCL, 0, 2, 0x4 },
        BitsRangeCase { 0xFFFFFFFFL, 29, 31, 0x7 },
        BitsRangeCase { 0x3BCL, 6, 6, 0x0 },
        BitsRangeCase { 0x3BCL, 5, 5, 0x1 },
        BitsRangeCase { 0xABCDE3BA29AFCCB7L, 43, 52, 0x1BC }
    ),
    BitsRangeCaseName
);
