#include <cstdio>
#include <gtest/gtest.h>
#include <unistd.h>

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
