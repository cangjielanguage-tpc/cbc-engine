#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "utils/logger.h"
#include "utils/options.h"

namespace {

using Opts = Options::Table;
using Opt = Options::Option;
using Status = Opts::Status;

struct EnvGuard {
    std::optional<std::string> saved;

    void Save() { saved = GetEnv(); }
    void Restore()
    {
        if (saved.has_value()) {
            setenv("CBCOPT", saved->c_str(), 1);
        } else {
            unsetenv("CBCOPT");
        }
    }

    static std::optional<std::string> GetEnv()
    {
        auto* val = std::getenv("CBCOPT");
        if (val == nullptr) {
            return std::nullopt;
        }
        return std::string(val);
    }
};

} // namespace

namespace {

TEST(OptionsSetters, SetBoolValue_True)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "true"), Status::OK);
    EXPECT_TRUE(var);
}

TEST(OptionsSetters, SetBoolValue_False)
{
    bool var = true;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "false"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetBoolValue_One)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "1"), Status::OK);
    EXPECT_TRUE(var);
}

TEST(OptionsSetters, SetBoolValue_Zero)
{
    bool var = true;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "0"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetBoolValue_Invalid)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "yes"), Status::INVALID_OPTION);
    EXPECT_FALSE(var);
}

TEST(OptionsSetters, SetStringValue)
{
    std::string var = "old";
    Opt opt = { "test.path", &var, &Options::SetStringValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.path", "/new/path"), Status::OK);
    EXPECT_EQ(var, "/new/path");
}

TEST(OptionsSetters, SetLogLevelValue)
{
    Logging::Logger logger;
    Opt opt = { "test.log", &logger, &Options::SetLogLevelValue };
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
    Opt opt = { "test.log", &logger, &Options::SetLogLevelValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.log", "invalid"), Status::INVALID_OPTION);
}

TEST(OptionsTable, UnknownOption)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.nonexistent", "x"), Status::UNKNOWN_OPTION);
}

TEST(OptionsTable, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    EXPECT_EQ(opts.Set("test.flag", "true"), Status::OK);
    EXPECT_EQ(opts.Set("test.flag", "false"), Status::OK);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, Simple)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
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
        { "test.flag", &flag, &Options::SetBoolValue },
        { "test.path", &path, &Options::SetStringValue },
    };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true test.path=/some/path", opts);

    EXPECT_TRUE(flag);
    EXPECT_EQ(path, "/some/path");
}

TEST(OptionsInitFromString, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true test.flag=false", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, Empty)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, LeadingTrailingSpaces)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
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
        { "test.flag", &var, &Options::SetBoolValue },
        { "test.str", &str, &Options::SetStringValue },
    };
    Opts opts(optsArray);

    Options::InitFromString("test.flag=true  test.str=hello", opts);
    EXPECT_TRUE(var);
    EXPECT_EQ(str, "hello");
}

TEST(OptionsInitFromString, MalformedKeyVal)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("noequalsign", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, UnknownOption)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.unknown=value", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, NoValueAfterEquals)
{
    bool var = false;
    Opt opt = { "test.foo", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.foo=", opts);
    EXPECT_FALSE(var);
}

TEST(OptionsInitFromString, ValueWithSpace)
{
    GTEST_SKIP() << "WIP";

    std::string var = "old";
    Opt opt = { "test.str", &var, &Options::SetStringValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    Options::InitFromString("test.str=string have space", opts);
    EXPECT_EQ(var, "string have space");
}

TEST(OptionsParseAndSet, Simple)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    const char* options[] = { "test.flag=true" };
    opts.ParseAndSet(1, options);

    EXPECT_TRUE(var);
}

TEST(OptionsParseAndSet, Nullptr)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    opts.ParseAndSet(0, nullptr);
}

TEST(OptionsParseAndSet, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    const char* options[] = { "test.flag=true", "test.flag=false" };
    opts.ParseAndSet(2, options);

    EXPECT_FALSE(var);
}

class InitFromEnvTest : public ::testing::Test {
protected:
    EnvGuard envGuard;

    void SetUp() override { envGuard.Save(); }
    void TearDown() override { envGuard.Restore(); }
};

TEST_F(InitFromEnvTest, Simple)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.flag=true", 1);
    Options::InitFromEnv(opts);

    EXPECT_TRUE(var);
}

TEST_F(InitFromEnvTest, Multiple)
{
    bool flag = false;
    std::string path;
    Opt optsArray[] = {
        { "test.flag", &flag, &Options::SetBoolValue },
        { "test.path", &path, &Options::SetStringValue },
    };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.flag=true test.path=/test/path", 1);
    Options::InitFromEnv(opts);

    EXPECT_TRUE(flag);
    EXPECT_EQ(path, "/test/path");
}

TEST_F(InitFromEnvTest, NotSet)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    unsetenv("CBCOPT");
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, Empty)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "", 1);
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, LastWins)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.flag=true test.flag=false", 1);
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, LeadingTrailingSpaces)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "  test.flag=true  ", 1);
    Options::InitFromEnv(opts);

    EXPECT_TRUE(var);
}

TEST_F(InitFromEnvTest, ConsecutiveSpaces)
{
    bool a = false;
    bool b = false;
    Opt optsArray[] = {
        { "test.a", &a, &Options::SetBoolValue },
        { "test.b", &b, &Options::SetBoolValue },
    };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.a=true  test.b=true", 1);
    Options::InitFromEnv(opts);

    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
}

TEST_F(InitFromEnvTest, UnknownOption)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.unknown=value", 1);
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, MalformedKeyVal)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "noequalsign", 1);
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, BoolZero)
{
    bool var = true;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.flag=0", 1);
    Options::InitFromEnv(opts);

    EXPECT_FALSE(var);
}

TEST_F(InitFromEnvTest, BoolOne)
{
    bool var = false;
    Opt opt = { "test.flag", &var, &Options::SetBoolValue };
    Opt optsArray[] = { opt };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.flag=1", 1);
    Options::InitFromEnv(opts);

    EXPECT_TRUE(var);
}

TEST_F(InitFromEnvTest, MixedTypes)
{
    Logging::Logger logLevel;
    bool flag = false;
    std::string str;

    Opt optsArray[] = {
        { "test.log", &logLevel, &Options::SetLogLevelValue },
        { "test.flag", &flag, &Options::SetBoolValue },
        { "test.str", &str, &Options::SetStringValue },
    };
    Opts opts(optsArray);

    setenv("CBCOPT", "test.log=debug test.flag=true test.str=test_value", 1);
    Options::InitFromEnv(opts);

    EXPECT_EQ(logLevel.GetLogLevel(), Logging::Level::DEBUG);
    EXPECT_TRUE(flag);
    EXPECT_EQ(str, "test_value");
}

TEST(OptionsMultiOptionTable, FromString)
{
    Logging::Logger logLevel1;
    Logging::Logger logLevel2;
    bool flag1 = false;
    bool flag2 = true;
    std::string str1;
    std::string str2;

    Opt optsArray[] = {
        { "test.log1", &logLevel1, &Options::SetLogLevelValue },
        { "test.log2", &logLevel2, &Options::SetLogLevelValue },
        { "test.flag1", &flag1, &Options::SetBoolValue },
        { "test.flag2", &flag2, &Options::SetBoolValue },
        { "test.str1", &str1, &Options::SetStringValue },
        { "test.str2", &str2, &Options::SetStringValue },
    };
    Opts opts(optsArray);

    Options::InitFromString(
        "test.log1=trace test.log2=warn test.flag1=1 test.flag2=0 test.str1=hello test.str2=world",
        opts
    );

    EXPECT_EQ(logLevel1.GetLogLevel(), Logging::Level::TRACE);
    EXPECT_EQ(logLevel2.GetLogLevel(), Logging::Level::WARN);
    EXPECT_TRUE(flag1);
    EXPECT_FALSE(flag2);
    EXPECT_EQ(str1, "hello");
    EXPECT_EQ(str2, "world");
}

} // namespace
