#include <gtest/gtest.h>

#include <string>

#include "utils/options.h"

namespace {

using Opts = Options::Table;
using Opt = Options::Option;
using Status = Opts::Status;

} // namespace

#include "utils/options_setup.h"

namespace {

TEST(OptionsSetters, SetBoolValue_True)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "true"), Status::OK);
    EXPECT_TRUE(var);
}

TEST(OptionsSetters, SetBoolValue_False)
{
    bool var = true;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "false"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetBoolValue_One)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "1"), Status::OK);
    EXPECT_TRUE(var);
}

TEST(OptionsSetters, SetBoolValue_Zero)
{
    bool var = true;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "0"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetBoolValue_Invalid)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "yes"), Status::INVALID_OPTION);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetStringValue)
{
    std::string var = "old";
    Opt opt = { "test.path", &var, &SetStringValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.path", "/new/path"), Status::OK);
    EXPECT_EQ(var, "/new/path");
}

TEST(OptionsSetters, SetLogLevelValue)
{
    Logging::Logger logger;
    Opt opt = { "test.log", &logger, &SetLogLevelValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.log", "trace"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::TRACE);

    EXPECT_EQ(opts.Set("test.log", "debug"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::DEBUG);

    EXPECT_EQ(opts.Set("test.log", "info"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::INFO);

    EXPECT_EQ(opts.Set("test.log", "warn"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::WARN);

    EXPECT_EQ(opts.Set("test.log", "error"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::ERROR);

    EXPECT_EQ(opts.Set("test.log", "fatal"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::FATAL);

    EXPECT_EQ(opts.Set("test.log", "none"), Status::OK);
    EXPECT_EQ(logger.GetLogLevel(), Logging::Level::NONE);
}

TEST(OptionsSetters, SetLogLevelValue_Invalid)
{
    Logging::Logger logger;
    Opt opt = { "test.log", &logger, &SetLogLevelValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.log", "invalid"), Status::INVALID_OPTION);
}

TEST(OptionsTable, UnknownOption)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.nonexistent", "x"), Status::UNKNOWN_OPTION);
}

TEST(OptionsTable, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "true"), Status::OK);
    EXPECT_EQ(opts.Set("test.flag", "false"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, Simple)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true", opts);
    EXPECT_TRUE(var);
}

TEST(OptionsInitFromString, Multiple)
{
    bool flag = false;
    std::string path;
    Opt optsArray[] = {
        { "test.flag", &flag, &SetBoolValue },
        { "test.path", &path, &SetStringValue },
    };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true test.path=/some/path", opts);

    EXPECT_TRUE(flag);
    EXPECT_EQ(path, "/some/path");
}

TEST(OptionsInitFromString, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true test.flag=false", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, Empty)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, LeadingTrailingSpaces)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("  test.flag=true  ", opts);
    EXPECT_TRUE(var);
}

TEST(OptionsInitFromString, ConsecutiveSpaces)
{
    bool var = false;
    std::string str;
    Opt optsArray[] = {
        { "test.flag", &var, &SetBoolValue },
        { "test.str", &str, &SetStringValue },
    };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true  test.str=hello", opts);
    EXPECT_TRUE(var);
    EXPECT_EQ(str, "hello");
}

TEST(OptionsInitFromString, MalformedKeyVal)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("noequalsign", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, UnknownOption)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.unknown=value", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsParseAndSet, Simple)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    const char* options[] = { "test.flag=true" };
    Options::ParseAndSet(1, options, opts);

    EXPECT_TRUE(var);
}

TEST(OptionsParseAndSet, Nullptr)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::ParseAndSet(0, nullptr, opts);
}

TEST(OptionsParseAndSet, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    const char* options[] = { "test.flag=true", "test.flag=false" };
    Options::ParseAndSet(2, options, opts);

    EXPECT_FALSE(var);
}

} // namespace
