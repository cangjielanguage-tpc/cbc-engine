#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "utils/ostream.h"

TEST(Stream, string)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("%s %d", "abc", 12);
    buf.PrintFmt(" %s %d", "cba", 23);
    ASSERT_EQ("abc 12 cba 23", buf.ToString());
    ASSERT_EQ(buf.ToString().size(), buf.Size());

    buf.Clear();
    ASSERT_EQ(0, buf.Size());
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

TEST(Stream, descripted_pipes)
{
    int pipes[2];
    pipe(pipes);
    int out = pipes[1];
    int in  = pipes[0];

    FILE* file = fdopen(out, "w");

    Stream::FileOutput fileStream(file);
    Stream::Descripted stream(fileStream, "[desc] ");
    stream.PrintFmt("%s %d", "abc", 12);
    stream.NewLine();
    stream.PrintFmt("%s %d", "cba", 23);
    stream.NewLine();

    stream.Flush();

    fclose(file);

    char buf[1024];
    auto n = read(in, buf, sizeof(buf));
    buf[n] = 0;
    close(in);

    auto expected = std::string("[desc] abc 12\n"
                                "[desc] cba 23\n");

    ASSERT_EQ(expected.size(), n);
    ASSERT_EQ(expected, buf);
}

// -------------------- Print("{}") formatting --------------------

TEST(Stream, print_braces_substitution)
{
    Stream::StringBuffer buf;
    buf.Print("a={} b={} c={}", 1, "two", '3');
    ASSERT_EQ("a=1 b=two c=3", buf.ToString());
}

TEST(Stream, print_braces_more_placeholders_than_args)
{
    Stream::StringBuffer buf;
    buf.Print("{}-{}", 1);
    ASSERT_EQ("1-", buf.ToString());
}

TEST(Stream, print_braces_more_args_than_placeholders)
{
    Stream::StringBuffer buf;
    buf.Print("[{}]", 1, 2, 3);
    ASSERT_EQ("[1]", buf.ToString());
}

TEST(Stream, print_no_placeholders_with_args)
{
    Stream::StringBuffer buf;
    buf.Print("plain", 5);
    ASSERT_EQ("plain", buf.ToString());
}

TEST(Stream, print_newline_in_format)
{
    Stream::StringBuffer buf;
    buf.Print("a\nb{}c", 1);
    ASSERT_EQ("a\nb1c", buf.ToString());
}

TEST(Stream, print_fmt_ln)
{
    Stream::StringBuffer buf;
    buf.PrintFmtLn("%d-%s", 5, "x");
    ASSERT_EQ("5-x\n", buf.ToString());
}

// -------------------- operator<< overloads --------------------

TEST(Stream, ostream_integers)
{
    Stream::StringBuffer buf;
    buf << (signed char)-1;
    buf << "|";
    buf << (unsigned char)255;
    buf << "|";
    buf << (short)-300;
    buf << "|";
    buf << (unsigned short)65535;
    buf << "|";
    buf << -123456;
    buf << "|";
    buf << 4294967295u;
    buf << "|";
    buf << -1234567890123456789L;
    buf << "|";
    buf << 18446744073709551615UL;
    buf << "|";
    buf << -9223372036854775807LL - 1;
    buf << "|";
    buf << 18446744073709551615ULL;
    ASSERT_EQ(
        "-1|255|-300|65535|-123456|4294967295|-1234567890123456789|"
        "18446744073709551615|-9223372036854775808|18446744073709551615",
        buf.ToString()
    );
}

TEST(Stream, ostream_integer_extremes)
{
    Stream::StringBuffer buf;
    buf << std::numeric_limits<int>::min();
    ASSERT_EQ("-2147483648", buf.ToString());
    buf.Clear();
    buf << std::numeric_limits<unsigned int>::max();
    ASSERT_EQ("4294967295", buf.ToString());
    buf.Clear();
    buf << std::numeric_limits<long long>::min();
    ASSERT_EQ("-9223372036854775808", buf.ToString());
    buf.Clear();
    buf << std::numeric_limits<unsigned long long>::max();
    ASSERT_EQ("18446744073709551615", buf.ToString());
}

TEST(Stream, ostream_bool_and_char)
{
    Stream::StringBuffer buf;
    buf << true << "|" << false << "|" << 'x' << "|" << 'y';
    ASSERT_EQ("true|false|x|y", buf.ToString());
}

TEST(Stream, ostream_strings)
{
    Stream::StringBuffer buf;
    buf << "cstr";
    buf << "|";
    buf << std::string("std-string");
    buf << "|";
    std::string longStr = "0123456789abcdef";
    buf << std::string_view(longStr.data() + 2, 5);
    ASSERT_EQ("cstr|std-string|23456", buf.ToString());
}

TEST(Stream, ostream_floats)
{
    Stream::StringBuffer buf;
    buf << 1.5f;
    buf << "|";
    buf << -2.25;
    buf << "|";
    buf << 3.5L;
    buf << "|";
    buf << 0.0;
    buf << "|";
    buf << -0.125f;
    ASSERT_EQ("1.500000|-2.250000|3.500000|0.000000|-0.125000", buf.ToString());
}

TEST(Stream, ostream_pointers)
{
    Stream::StringBuffer buf;
    int value = 0;
    const void* p = &value;
    buf << p;
    std::string viaVoid = buf.ToString();
    ASSERT_FALSE(viaVoid.empty());

    buf.Clear();
    buf << (const int*)&value;
    ASSERT_EQ(viaVoid, buf.ToString());

    buf.Clear();
    buf << (const signed char*)&value;
    buf << (const unsigned char*)&value;
    buf << (const short*)&value;
    buf << (const unsigned short*)&value;
    buf << (const unsigned int*)&value;
    buf << (const long*)&value;
    buf << (const unsigned long*)&value;
    buf << (const long long*)&value;
    buf << (const unsigned long long*)&value;
    ASSERT_FALSE(buf.ToString().empty());

    buf.Clear();
    const long* nullLong = nullptr;
    buf << nullLong;
    ASSERT_FALSE(buf.ToString().empty());
}

TEST(Stream, ostream_endl)
{
    Stream::StringBuffer buf;
    buf << "a" << Stream::endl << "b";
    ASSERT_EQ("a\nb", buf.ToString());
}

TEST(Stream, ostream_pair)
{
    Stream::StringBuffer buf;
    buf << std::make_pair(1, 2);
    buf << "|";
    buf << std::make_pair("x", std::make_pair(2, 'y'));
    ASSERT_EQ("(1, 2)|(x, (2, y))", buf.ToString());
}

TEST(Stream, hex_format)
{
    Stream::StringBuffer buf;
    buf << Stream::Hex(0xAB);
    buf << "|" << Stream::Hex(0);
    buf << "|" << Stream::Hex(0xdeadbeefULL);
    ASSERT_EQ("ab|0|deadbeef", buf.ToString());
}

TEST(Stream, hex_negative_and_pointer)
{
    Stream::StringBuffer buf;
    buf << Stream::Hex(-1);
    ASSERT_EQ("ffffffffffffffff", buf.ToString());
    buf.Clear();
    buf << Stream::Hex((int16_t)-2);
    ASSERT_EQ("fffffffffffffffe", buf.ToString());
    buf.Clear();
    void* p = &buf;
    char expected[32];
    snprintf(expected, sizeof(expected), "%llx", (unsigned long long)(uintptr_t)p);
    buf << Stream::Hex(p);
    ASSERT_EQ(std::string(expected), buf.ToString());
}

// -------------------- StringBuffer capacity growth --------------------

TEST(Stream, string_buffer_growth_across_reallocs)
{
    Stream::StringBuffer buf;
    std::string expected;
    for (int i = 0; i < 200; i++) {
        char piece[16];
        snprintf(piece, sizeof(piece), "%03d,", i);
        buf.PrintFmt("%s", piece);
        expected += piece;
    }
    ASSERT_EQ(expected.size(), buf.Size());
    ASSERT_EQ(expected, buf.ToString());
}

TEST(Stream, string_buffer_single_write_larger_than_initial_capacity)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("%0300x", 0xABC);
    ASSERT_EQ(300u, buf.Size());
    std::string text = buf.ToString();
    ASSERT_EQ(300u, text.size());
    ASSERT_EQ(std::string(297, '0') + "abc", text);
}

TEST(Stream, string_buffer_tostring_fresh_empty)
{
    Stream::StringBuffer buf;
    ASSERT_EQ(0u, buf.Size());
    ASSERT_EQ("", buf.ToString());
}

TEST(Stream, string_buffer_tostring_after_clear)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("some data");
    buf.Clear();
    ASSERT_EQ(0u, buf.Size());
    ASSERT_EQ("", buf.ToString());
    buf.PrintFmt("more");
    ASSERT_EQ("more", buf.ToString());
}

TEST(Stream, string_buffer_cstring)
{
    Stream::StringBuffer buf;
    buf.PrintFmt("hello %d", 42);
    char* c = buf.ToCString();
    ASSERT_NE(nullptr, c);
    EXPECT_STREQ("hello 42", c);
    ASSERT_EQ(strlen(c), buf.Size());
    free(c);
}

TEST(Stream, string_buffer_cstring_fresh_empty)
{
    Stream::StringBuffer buf;
    char* c = buf.ToCString();
    ASSERT_NE(nullptr, c);
    EXPECT_STREQ("", c);
    free(c);
}

// -------------------- ThreadBufferedOutput --------------------

namespace {

class LineCollector : public Stream::ThreadBufferedOutput {
public:
    void EmitLine(const std::string& line) const override { lines.push_back(line); }

    mutable std::vector<std::string> lines;
};

} // namespace

TEST(Stream, thread_buffered_buffers_until_newline)
{
    LineCollector out;
    out.PrintFmt("abc");
    out.PrintFmt("%d", 42);
    ASSERT_TRUE(out.lines.empty());
    out.NewLine();
    ASSERT_EQ(1u, out.lines.size());
    ASSERT_EQ("abc42", out.lines[0]);
}

TEST(Stream, thread_buffered_flush_emits_partial_line)
{
    LineCollector out;
    out.Print("next");
    out.Flush();
    ASSERT_EQ(1u, out.lines.size());
    ASSERT_EQ("next", out.lines[0]);

    out.Flush();
    ASSERT_EQ(1u, out.lines.size());
}

TEST(Stream, thread_buffered_flush_empty_does_nothing)
{
    LineCollector out;
    out.Flush();
    ASSERT_TRUE(out.lines.empty());
}

TEST(Stream, thread_buffered_endl_operator)
{
    LineCollector out;
    out << "v" << Stream::endl << "w" << Stream::endl;
    ASSERT_EQ(2u, out.lines.size());
    ASSERT_EQ("v", out.lines[0]);
    ASSERT_EQ("w", out.lines[1]);
}

// -------------------- Indented --------------------

TEST(Stream, indent_set_and_get)
{
    Stream::StringBuffer buf;
    Stream::Indented stream(buf, 1);
    ASSERT_EQ(1u, stream.GetIndent());
    stream.Print("a");
    stream.NewLine();
    stream.SetIndent(3);
    stream.Print("b");
    stream.NewLine();
    ASSERT_EQ(
        " a\n"
        "   b\n",
        buf.ToString()
    );
}

TEST(Stream, descripted_empty_prefix)
{
    Stream::StringBuffer buf;
    Stream::Descripted stream(buf, "");
    stream.PrintFmt("%d", 7);
    stream.NewLine();
    stream.PrintFmt("%d", 8);
    stream.NewLine();
    ASSERT_EQ(
        "7\n"
        "8\n",
        buf.ToString()
    );
}
