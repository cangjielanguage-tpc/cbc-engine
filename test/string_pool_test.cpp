#include "utils/string_pool.h"
#include <gtest/gtest.h>


TEST(StringPool, Simple)
{
    Utils::StringPool pool;
    auto asd = pool.InternAndGetId("asd");
    auto qwe = pool.InternAndGetId("qwe");
    auto asd2 = pool.InternAndGetId("asd");
    EXPECT_NE(asd, qwe);
    EXPECT_NE(asd2, qwe);
    EXPECT_EQ(asd, asd2);
}
