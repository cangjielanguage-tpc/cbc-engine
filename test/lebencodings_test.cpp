#include "utils/lebencodings.h"

#include <climits>
#include <cstdint>
#include <gtest/gtest.h>

// assertions must appear inside of `TEST` body.

#define TEST_PAIR_NAMED(name, value, kind)                                                                             \
    TEST(Leb, name)                                                                                                    \
    {                                                                                                                  \
        char buf[LEB::MAX_SIZE];                                                                                       \
        char* p1 = buf;                                                                                                \
        char* p2 = buf;                                                                                                \
        LEB::Encode##kind((value), &p1, buf + sizeof(buf));                                                            \
        auto val = LEB::Decode##kind(&p2, buf + sizeof(buf));                                                          \
        ASSERT_EQ((value), val);                                                                                       \
    }

#define TEST_PAIR(value, kind) TEST_PAIR_NAMED(kind##value, (value), kind)

TEST_PAIR(0x0, SLEB)
TEST_PAIR(0x1, SLEB)
TEST_PAIR(0x20, SLEB)
TEST_PAIR(0x3000, SLEB)
TEST_PAIR(0x40000, SLEB)
TEST_PAIR(0x500000, SLEB)
TEST_PAIR(0x6000000, SLEB)
TEST_PAIR(0x70000000, SLEB)
TEST_PAIR(0x800000000, SLEB)
TEST_PAIR(0x9000000000, SLEB)
TEST_PAIR(0xa0000000000, SLEB)
TEST_PAIR(0xb00000000000, SLEB)
TEST_PAIR(0xc000000000000, SLEB)
TEST_PAIR(0xd0000000000000, SLEB)
TEST_PAIR(0xe00000000000000, SLEB)
TEST_PAIR(0xf000000000000000, SLEB)

TEST_PAIR_NAMED(neg0x1, -0x1, SLEB)
TEST_PAIR_NAMED(neg0x20, -0x20, SLEB)
TEST_PAIR_NAMED(neg0x3000, -0x3000, SLEB)
TEST_PAIR_NAMED(neg0x40000, -0x40000, SLEB)
TEST_PAIR_NAMED(neg0x500000, -0x500000, SLEB)
TEST_PAIR_NAMED(neg0x6000000, -0x6000000, SLEB)
TEST_PAIR_NAMED(neg0x70000000, -0x70000000, SLEB)
TEST_PAIR_NAMED(neg0x800000000, -0x800000000, SLEB)
TEST_PAIR_NAMED(neg0x9000000000, -0x9000000000, SLEB)
TEST_PAIR_NAMED(neg0xa0000000000, -0xa0000000000, SLEB)
TEST_PAIR_NAMED(neg0xb00000000000, -0xb00000000000, SLEB)
TEST_PAIR_NAMED(neg0xc000000000000, -0xc000000000000, SLEB)
TEST_PAIR_NAMED(neg0xd0000000000000, -0xd0000000000000, SLEB)
TEST_PAIR_NAMED(neg0xe00000000000000, -0xe00000000000000, SLEB)
TEST_PAIR_NAMED(neg0xf000000000000000, -0xf000000000000000, SLEB)

TEST_PAIR(0x0, ULEB)
TEST_PAIR(0x1, ULEB)
TEST_PAIR(0x20, ULEB)
TEST_PAIR(0x3000, ULEB)
TEST_PAIR(0x40000, ULEB)
TEST_PAIR(0x500000, ULEB)
TEST_PAIR(0x6000000, ULEB)
TEST_PAIR(0x70000000, ULEB)
TEST_PAIR(0x800000000, ULEB)
TEST_PAIR(0x9000000000, ULEB)
TEST_PAIR(0xa0000000000, ULEB)
TEST_PAIR(0xb00000000000, ULEB)
TEST_PAIR(0xc000000000000, ULEB)
TEST_PAIR(0xd0000000000000, ULEB)
TEST_PAIR(0xe00000000000000, ULEB)
TEST_PAIR(0xf000000000000000, ULEB)

TEST_PAIR(UINT32_MAX, SLEB)
TEST_PAIR(INT32_MAX, SLEB)
TEST_PAIR(INT64_MAX, SLEB)
TEST_PAIR(INT64_MIN, SLEB)
TEST_PAIR(INT32_MIN, SLEB)
TEST_PAIR(0x7f, SLEB)
TEST_PAIR(0x80, SLEB)

TEST_PAIR(UINT32_MAX, ULEB)
TEST_PAIR(INT32_MAX, ULEB)
TEST_PAIR(INT64_MAX, ULEB)
TEST_PAIR(INT64_MIN, ULEB)
TEST_PAIR(INT32_MIN, ULEB)
TEST_PAIR(0x7f, ULEB)
TEST_PAIR(0x80, ULEB)
