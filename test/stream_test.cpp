#include <gtest/gtest.h>

#include "utils/ostream.h"

TEST(Stream, string)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("%s %d", "abc", 12);
    buf.PrintFmt(" %s %d", "cba", 23);
    ASSERT_EQ("abc 12 cba 23", buf.ToString());
}
