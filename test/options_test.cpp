#include <gtest/gtest.h>

#include <charconv>
#include <string>

#include "cbc/isa_disasm.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/logger.h"
#include "utils/options.h"

namespace {

class OptionsTest : public ::testing::Test {
protected:
    Options::Table::Snapshot saved;

    void SetUp() override { saved = Options::g_table.SaveContext(); }
    void TearDown() override { Options::g_table.RestoreContext(saved); }
};

} // namespace

TEST_F(OptionsTest, ParseAndSet)
{
    const char* options[] = {
        "cbc.log.resolution=trace", "cbc.log.int=info", "cbc.dasm=true", "cbc.path=/path/to/cbc/sources"
    };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::TRACE);
    EXPECT_EQ(Interpretation::Log::interpretation.GetLogLevel(), Logging::Level::INFO);
    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(g_cbcPath, "/path/to/cbc/sources");
}

TEST_F(OptionsTest, ParseAndSet_Nullptr)
{
    Options::ParseAndSet(0, nullptr, Options::g_table);
}

TEST_F(OptionsTest, ParseAndSet_Empty)
{
    const char* options[] = { "cbc.log.resolution=trace" };
    Options::ParseAndSet(0, options, Options::g_table);
}

TEST_F(OptionsTest, ParseAndSet_UnknownOption)
{
    const char* options[] = { "cbc.nonexistent=value" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);
}

TEST_F(OptionsTest, ParseAndSet_InvalidBoolValue)
{
    bool savedDasm = Cbc::g_IsRawDisasmEnabled;
    const char* options[] = { "cbc.dasm=yes" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);

    EXPECT_EQ(Cbc::g_IsRawDisasmEnabled, savedDasm);
}

TEST_F(OptionsTest, ParseAndSet_MalformedKeyVal)
{
    const char* options[] = { "noequalsign" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);
}

TEST_F(OptionsTest, ParseAndSet_NoValue)
{
    bool savedDasm = Cbc::g_IsRawDisasmEnabled;
    const char* options[] = { "cbc.dasm=" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);

    EXPECT_EQ(Cbc::g_IsRawDisasmEnabled, savedDasm);
}

TEST_F(OptionsTest, ParseAndSet_MultipleKeysLastWins)
{
    const char* options[] = { "cbc.dasm=true", "cbc.dasm=false" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);

    EXPECT_FALSE(Cbc::g_IsRawDisasmEnabled);
}

TEST_F(OptionsTest, ParseAndSet_AllLogLevels)
{
    const char* options[] = { "cbc.log.all=debug" };
    constexpr size_t optionsCount = std::size(options);

    Options::ParseAndSet(static_cast<int>(optionsCount), options, Options::g_table);

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::DEBUG);
    EXPECT_EQ(Interpretation::Log::interpretation.GetLogLevel(), Logging::Level::DEBUG);
    EXPECT_EQ(Interpretation::Log::preparation.GetLogLevel(), Logging::Level::DEBUG);
}

TEST_F(OptionsTest, InitFromString)
{
    Options::InitFromString("cbc.log.resolution=trace cbc.dasm=true cbc.path=/test/path", Options::g_table);

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::TRACE);
    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(g_cbcPath, "/test/path");
}

TEST_F(OptionsTest, InitFromString_Empty)
{
    Options::InitFromString("", Options::g_table);
}

TEST_F(OptionsTest, InitFromString_LeadingTrailingSpaces)
{
    Options::InitFromString("  cbc.dasm=true  ", Options::g_table);

    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
}

TEST_F(OptionsTest, InitFromString_ConsecutiveSpaces)
{
    Options::InitFromString("cbc.dasm=true  cbc.log.resolution=warn", Options::g_table);

    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::WARN);
}

TEST_F(OptionsTest, InitFromString_UnknownOption)
{
    Options::InitFromString("cbc.nonexistent=value", Options::g_table);
}

TEST_F(OptionsTest, InitFromString_Malformed)
{
    Options::InitFromString("noequalsign", Options::g_table);
}

TEST_F(OptionsTest, InitFromString_BoolZero)
{
    Options::InitFromString("cbc.dasm=0", Options::g_table);

    EXPECT_FALSE(Cbc::g_IsRawDisasmEnabled);
}

TEST_F(OptionsTest, InitFromString_BoolOne)
{
    Cbc::g_IsRawDisasmEnabled = false;
    Options::InitFromString("cbc.dasm=1", Options::g_table);

    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
}

// --- Custom Table instance tests ---

namespace {

using Opts = Options::Table;
using Opt = Options::Option;
using Status = Opts::Status;

bool SetIntValue(Opts const&, Opt const& opt, std::string_view value)
{
    int val;
    auto res = std::from_chars(value.begin(), value.end(), val);
    if (res.ptr == value.end()) {
        *reinterpret_cast<int*>(opt.location) = val;
        return true;
    }
    return false;
}

bool SetStringVal(Opts const&, Opt const& opt, std::string_view value)
{
    *reinterpret_cast<std::string*>(opt.location) = value;
    return true;
}

} // namespace

TEST(OptionsCustom, SetAndGet)
{
    int myCounter = 0;
    std::string myLabel;

    Opt fakeOpts[] = {
        { "test.counter", &myCounter, &SetIntValue },
        { "test.label", &myLabel, &SetStringVal },
    };
    Opts opts(fakeOpts);

    EXPECT_EQ(opts.Set("test.counter", "42"), Status::OK);
    EXPECT_EQ(myCounter, 42);

    EXPECT_EQ(opts.Set("test.label", "hello"), Status::OK);
    EXPECT_EQ(myLabel, "hello");
}

TEST(OptionsCustom, UnknownOption)
{
    int var = 0;
    Opt fakeOpts[] = {
        { "test.var", &var, &SetIntValue },
    };
    Opts opts(fakeOpts);

    EXPECT_EQ(opts.Set("test.nonexistent", "x"), Status::UNKNOWN_OPTION);
    EXPECT_EQ(var, 0);
}

TEST(OptionsCustom, InvalidValue)
{
    int var = 0;
    Opt fakeOpts[] = {
        { "test.var", &var, &SetIntValue },
    };
    Opts opts(fakeOpts);

    EXPECT_EQ(opts.Set("test.var", "notanumber"), Status::INVALID_OPTION);
    EXPECT_EQ(var, 0);
}

TEST(OptionsCustom, MultipleOptions)
{
    int a = 0;
    int b = 0;
    Opt fakeOpts[] = {
        { "test.a", &a, &SetIntValue },
        { "test.b", &b, &SetIntValue },
    };
    Opts opts(fakeOpts);

    EXPECT_EQ(opts.Set("test.a", "10"), Status::OK);
    EXPECT_EQ(opts.Set("test.b", "20"), Status::OK);
    EXPECT_EQ(a, 10);
    EXPECT_EQ(b, 20);
}

TEST(OptionsCustom, LastWins)
{
    int var = 0;
    Opt fakeOpts[] = {
        { "test.var", &var, &SetIntValue },
    };
    Opts opts(fakeOpts);

    EXPECT_EQ(opts.Set("test.var", "1"), Status::OK);
    EXPECT_EQ(opts.Set("test.var", "2"), Status::OK);
    EXPECT_EQ(var, 2);
}

TEST(OptionsCustom, InitFromString)
{
    int var = 0;
    Opt fakeOpts[] = {
        { "test.var", &var, &SetIntValue },
    };
    Opts opts(fakeOpts);

    Options::InitFromString("test.var=42", opts);

    EXPECT_EQ(var, 42);
}

TEST(OptionsCustom, InitFromString_LastWins)
{
    int var = 0;
    Opt fakeOpts[] = {
        { "test.var", &var, &SetIntValue },
    };
    Opts opts(fakeOpts);

    Options::InitFromString("test.var=10 test.var=20", opts);

    EXPECT_EQ(var, 20);
}

TEST(OptionsCustom, InitFromString_MultipleOptions)
{
    int a = 0;
    int b = 0;
    Opt fakeOpts[] = {
        { "test.a", &a, &SetIntValue },
        { "test.b", &b, &SetIntValue },
    };
    Opts opts(fakeOpts);

    Options::InitFromString("test.a=100 test.b=200", opts);

    EXPECT_EQ(a, 100);
    EXPECT_EQ(b, 200);
}
