#include "utils/lebencodings.h"

#include <cstdint>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>

namespace {

std::string SlebCaseName(const testing::TestParamInfo<int64_t>& info)
{
    char buf[48];
    uint64_t magnitude = info.param < 0 ? 0ull - static_cast<uint64_t>(info.param) : static_cast<uint64_t>(info.param);
    snprintf(buf, sizeof(buf), "%s%llu", info.param < 0 ? "neg" : "v", static_cast<unsigned long long>(magnitude));
    return buf;
}

std::string UlebCaseName(const testing::TestParamInfo<uint64_t>& info)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "v%llX", static_cast<unsigned long long>(info.param));
    return buf;
}

class LebSLEBRoundTrip : public testing::TestWithParam<int64_t> {};

TEST_P(LebSLEBRoundTrip, EncodeDecode)
{
    auto value = GetParam();
    char buf[LEB::MAX_SIZE];
    char* encodePtr = buf;
    char* decodePtr = buf;
    LEB::EncodeSLEB(value, &encodePtr, buf + sizeof(buf));
    auto decoded = LEB::DecodeSLEB(&decodePtr, buf + sizeof(buf));
    ASSERT_EQ(value, decoded);
    ASSERT_EQ(encodePtr - buf, decodePtr - buf);
}

INSTANTIATE_TEST_SUITE_P(
    Values,
    LebSLEBRoundTrip,
    testing::Values(
        0x0,
        0x1,
        0x20,
        0x3F,
        0x40,
        -0x1,
        -0x20,
        -0x40,
        -0x41,
        0x1FFF,
        0x2000,
        -0x2000,
        -0x2001,
        0x1FFFFF,
        0x200000,
        -0x200000,
        -0x200001,
        0x3000,
        0x40000,
        0x500000,
        0x6000000,
        0x70000000,
        0x800000000,
        0x9000000000,
        0xa0000000000,
        0xb00000000000,
        0xc000000000000,
        0xd0000000000000,
        0xe00000000000000,
        static_cast<int64_t>(0xf000000000000000ull),
        0x7F,
        0x80,
        INT32_MAX,
        INT32_MIN,
        UINT32_MAX,
        INT64_MAX,
        INT64_MIN
    ),
    SlebCaseName
);

class LebULEBRoundTrip : public testing::TestWithParam<uint64_t> {};

TEST_P(LebULEBRoundTrip, EncodeDecode)
{
    auto value = GetParam();
    char buf[LEB::MAX_SIZE];
    char* encodePtr = buf;
    char* decodePtr = buf;
    LEB::EncodeULEB(value, &encodePtr, buf + sizeof(buf));
    auto decoded = LEB::DecodeULEB(&decodePtr, buf + sizeof(buf));
    ASSERT_EQ(value, decoded);
    ASSERT_EQ(encodePtr - buf, decodePtr - buf);
}

INSTANTIATE_TEST_SUITE_P(
    Values,
    LebULEBRoundTrip,
    testing::Values(
        0x0ull,
        0x1ull,
        0x20ull,
        0x3Full,
        0x40ull,
        0x7Full,
        0x80ull,
        0xFFull,
        0x100ull,
        0x3FFFull,
        0x4000ull,
        0x1FFFFFull,
        0x20000ull,
        0x3000ull,
        0x40000ull,
        0x500000ull,
        0x6000000ull,
        0x70000000ull,
        0x800000000ull,
        0x9000000000ull,
        0xa0000000000ull,
        0xb00000000000ull,
        0xc000000000000ull,
        0xd0000000000000ull,
        0xe00000000000000ull,
        0xf000000000000000ull,
        UINT32_MAX,
        INT64_MAX,
        1ull << 63,
        UINT64_MAX
    ),
    UlebCaseName
);

} // namespace
