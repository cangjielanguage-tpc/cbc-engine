#include <gtest/gtest.h>

#include "utils/ostream.h"

TEST(Stream, string)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("%s %d", "abc", 12);
    buf.PrintFmt(" %s %d", "cba", 23);
    ASSERT_EQ("abc 12 cba 23", buf.ToString());
}

TEST(Stream, descripted)
{
    Stream::StringBuffer buf;
    Stream::Descripted stream(buf, "[desc] ");
    stream.PrintFmt("%s %d", "abc", 12);
    stream.NewLine();
    stream.PrintFmt("%s %d", "cba", 23);
    stream.NewLine();
    ASSERT_EQ(
        "[desc] abc 12\n"
        "[desc] cba 23\n",
        buf.ToString()
    );
}

TEST(Stream, indent)
{
    Stream::StringBuffer buf;
    Stream::Indented stream(buf, 2);
    stream.PrintFmt("%s %d", "abc", 12);
    stream.NewLine();
    buf.Print("--");
    buf.NewLine();
    stream.PrintFmt("%s %d", "cba", 23);
    stream.NewLine();
    buf.Print("--");
    buf.NewLine();
    ASSERT_EQ(
        "  abc 12\n"
        "--\n"
        "  cba 23\n"
        "--\n",
        buf.ToString()
    );
}
