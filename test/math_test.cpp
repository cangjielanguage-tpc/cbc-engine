#include "utils/math.h"
#include <gtest/gtest.h>

TEST(MathUtils, IsNBits)
{
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 0));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 1));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 4));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 8));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 16));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 32));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0x0L, 64));

    ASSERT_TRUE(!MathUtils::IsNBits(0xFL, 0));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFL, 1));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 4));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 8));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 16));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 32));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0xFL, 64));

    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFL, 0));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFL, 1));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFL, 4));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFL, 8));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFL, 16));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFL, 32));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFL, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFL, 64));

    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFL, 0));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFL, 1));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFL, 4));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFL, 8));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFL, 16));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFL, 32));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFL, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFL, 64));

    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 0));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 1));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 4));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 8));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 16));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFL, 32));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFFFFFL, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFFFFFL, 64));

    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 0));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 1));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 4));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 8));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 16));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 32));
    ASSERT_TRUE(!MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 48));
    ASSERT_TRUE(MathUtils::IsNBits(0xFFFFFFFFFFFFFFFFL, 64));
}

TEST(MathUtils, IsNBitsSigned32)
{
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int32_t>(0xF, 2));
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int32_t>(0xF, 4));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xF, 5));

    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int32_t>(0xFF, 4));
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int32_t>(0xFF, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xFF, 9));

    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xFFFFFFFF, 2));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xFFFFFFFF, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xFFFFFFFF, 32));

    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int32_t>(0xFFFFFF37, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int32_t>(0xFFFFFF37, 9));
}

TEST(MathUtils, IsNBitsSigned64)
{
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int64_t>(0xFL, 2));
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int64_t>(0xFL, 4));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFL, 5));

    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int64_t>(0xFFL, 4));
    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int64_t>(0xFFL, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFFL, 9));

    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFFFFFFFFFFFFFFFFL, 2));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFFFFFFFFFFFFFFFFL, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFFFFFFFFFFFFFFFFL, 32));

    ASSERT_TRUE(!MathUtils::IsNBitsSigned<int64_t>(0xFFFFFFFFFFFFFF37L, 8));
    ASSERT_TRUE(MathUtils::IsNBitsSigned<int64_t>(0xFFFFFFFFFFFFFF37L, 9));
}

TEST(MathUtils, RightNBits32)
{
    ASSERT_EQ(0x0, MathUtils::RightNBits32(0));
    ASSERT_EQ(0x7, MathUtils::RightNBits32(3));
    ASSERT_EQ(0xFF, MathUtils::RightNBits32(8));
    ASSERT_EQ(0xFFF, MathUtils::RightNBits32(12));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::RightNBits32(32));
}

TEST(MathUtils, RightNBits64)
{
    ASSERT_EQ(0x0, MathUtils::RightNBits64(0));
    ASSERT_EQ(0x7, MathUtils::RightNBits64(3));
    ASSERT_EQ(0xFF, MathUtils::RightNBits64(8));
    ASSERT_EQ(0xFFF, MathUtils::RightNBits64(12));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::RightNBits64(32));
    ASSERT_EQ(0xFFFFFFFFFFFF, MathUtils::RightNBits64(48));
    ASSERT_EQ(0xFFFFFFFFFFFFFFFF, MathUtils::RightNBits64(64));
}

TEST(MathUtils, SignExtend32)
{
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint32_t)0x0, 1));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint32_t)0x0, 2));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint32_t)0x0, 28));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint32_t)0x0, 32));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::SignExtend((uint32_t)0xFFFF, 16));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::SignExtend((uint32_t)0xFFFFFFFF, 32));

    ASSERT_EQ(-1, MathUtils::SignExtend((uint32_t)0x1, 1));
    ASSERT_EQ(1, MathUtils::SignExtend((uint32_t)0x1, 2));

    ASSERT_EQ(0x9, MathUtils::SignExtend((uint32_t)0x9, 14));

    ASSERT_EQ(0xFFFFFFF9, MathUtils::SignExtend((uint32_t)0x9, 4));
}

TEST(MathUtils, SignExtend64)
{
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint64_t)0x0, 1));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint64_t)0x0, 2));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint64_t)0x0, 63));
    ASSERT_EQ(0x0, MathUtils::SignExtend((uint64_t)0x0, 64));
    ASSERT_EQ(0xFFFFFFFFFFFFFFFF, MathUtils::SignExtend((uint64_t)0xFFFF, 16));
    ASSERT_EQ(0xFFFFFFFFFFFFFFFF, MathUtils::SignExtend((uint64_t)0xFFFFFFFF, 32));
    ASSERT_EQ(0xFFFFFFFFFFFFFFFF, MathUtils::SignExtend((uint64_t)0xFFFFFFFFFFFFFFFF, 64));

    ASSERT_EQ(-1, MathUtils::SignExtend((uint64_t)0x1, 1));
    ASSERT_EQ(1, MathUtils::SignExtend((uint64_t)0x1, 2));

    ASSERT_EQ(0x9, MathUtils::SignExtend((uint64_t)0x9, 14));

    ASSERT_EQ(0xFFFFFFFFFFFFFFF9, MathUtils::SignExtend((uint64_t)0x9, 4));
}

TEST(MathUtils, ZeroExtend32)
{
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint32_t)0x0, 1));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint32_t)0x0, 2));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint32_t)0x0, 28));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint32_t)0x0, 32));
    ASSERT_EQ(0xFFFF, MathUtils::ZeroExtend((uint32_t)0xFFFF, 16));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::ZeroExtend((uint32_t)0xFFFFFFFF, 32));

    ASSERT_EQ(1, MathUtils::ZeroExtend((uint32_t)0x1, 1));
    ASSERT_EQ(1, MathUtils::ZeroExtend((uint32_t)0x1, 2));

    ASSERT_EQ(0x9, MathUtils::ZeroExtend((uint32_t)0x9, 14));
    ASSERT_EQ(0x9, MathUtils::ZeroExtend((uint32_t)0x9, 4));
}

TEST(MathUtils, ZeroExtend64)
{
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint64_t)0x0, 1));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint64_t)0x0, 2));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint64_t)0x0, 63));
    ASSERT_EQ(0x0, MathUtils::ZeroExtend((uint64_t)0x0, 64));
    ASSERT_EQ(0xFFFF, MathUtils::ZeroExtend((uint64_t)0xFFFF, 16));
    ASSERT_EQ(0xFFFFFFFF, MathUtils::ZeroExtend((uint64_t)0xFFFFFFFF, 32));
    ASSERT_EQ(0xFFFFFFFFFFFFFFFF, MathUtils::ZeroExtend((uint64_t)0xFFFFFFFFFFFFFFFF, 64));

    ASSERT_EQ(1, MathUtils::ZeroExtend((uint64_t)0x1, 1));
    ASSERT_EQ(1, MathUtils::ZeroExtend((uint64_t)0x1, 2));

    ASSERT_EQ(0x9, MathUtils::ZeroExtend((uint64_t)0x9, 14));
    ASSERT_EQ(0x9, MathUtils::ZeroExtend((uint64_t)0x9, 4));
}

TEST(MathUtils, Bits64)
{
    ASSERT_EQ(0x5L, MathUtils::Bits(0x3EFL, 3, 5));
    ASSERT_EQ(0x5L, MathUtils::Bits(0x2F0L, 7, 9));

    ASSERT_EQ(0x4L, MathUtils::Bits(0x3FCL, 0, 2));
    ASSERT_EQ(0x7L, MathUtils::Bits(0xFFFFFFFFL, 29, 31));

    ASSERT_EQ(0x0L, MathUtils::Bits(0x3BCL, 6, 6));
    ASSERT_EQ(0x1L, MathUtils::Bits(0x3BCL, 5, 5));

    ASSERT_EQ(0x1BCL, MathUtils::Bits(0xABCDE3BA29AFCCB7L, 43, 52));
}
