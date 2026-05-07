#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "cbc/isa_disasm.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"

#include "utils/logger.h"
#include "utils/options.h"

namespace {

struct SavedState {
    Logging::Level resLog;
    Logging::Level intLog;
    Logging::Level prepLog;
    bool dasm;
    std::string cbcPath;
    std::string mainCbc;
};

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

SavedState SaveState()
{
    return {
        Resolution::log.GetLogLevel(),
        Interpretation::Log::interpretation.GetLogLevel(),
        Interpretation::Log::preparation.GetLogLevel(),
        Cbc::g_IsRawDisasmEnabled,
        g_cbcPath,
        g_mainCbc,
    };
}

void RestoreState(SavedState const& s)
{
    Resolution::log.SetLogLevel(s.resLog);
    Interpretation::Log::interpretation.SetLogLevel(s.intLog);
    Interpretation::Log::preparation.SetLogLevel(s.prepLog);
    Cbc::g_IsRawDisasmEnabled = s.dasm;
    g_cbcPath = s.cbcPath;
    g_mainCbc = s.mainCbc;
}

class OptionsTest : public ::testing::Test {
protected:
    SavedState saved;
    EnvGuard envGuard;

    void SetUp() override
    {
        saved = SaveState();
        envGuard.Save();
    }

    void TearDown() override
    {
        RestoreState(saved);
        envGuard.Restore();
    }
};

} // namespace

TEST_F(OptionsTest, ParseAndSetOptions)
{
    const char* options[] = {
        "cbc.log.resolution=trace", "cbc.log.int=info", "cbc.dasm=true", "cbc.path=/path/to/cbc/sources"
    };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::TRACE);
    EXPECT_EQ(Interpretation::Log::interpretation.GetLogLevel(), Logging::Level::INFO);
    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(g_cbcPath, "/path/to/cbc/sources");
}

TEST_F(OptionsTest, ParseAndSetOptions_Nullptr)
{
    Options::ParseAndSetOptions(0, nullptr);
}

TEST_F(OptionsTest, ParseAndSetOptions_Empty)
{
    const char* options[] = { "cbc.log.resolution=trace" };
    Options::ParseAndSetOptions(0, options);

    EXPECT_EQ(Resolution::log.GetLogLevel(), saved.resLog);
}

TEST_F(OptionsTest, ParseAndSetOptions_UnknownOption)
{
    const char* options[] = { "cbc.nonexistent=value" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Resolution::log.GetLogLevel(), saved.resLog);
    EXPECT_EQ(g_cbcPath, saved.cbcPath);
}

TEST_F(OptionsTest, ParseAndSetOptions_InvalidBoolValue)
{
    bool savedDasm = Cbc::g_IsRawDisasmEnabled;
    const char* options[] = { "cbc.dasm=yes" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Cbc::g_IsRawDisasmEnabled, savedDasm);
}

TEST_F(OptionsTest, ParseAndSetOptions_MalformedKeyVal)
{
    const char* options[] = { "noequalsign" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Resolution::log.GetLogLevel(), saved.resLog);
}

TEST_F(OptionsTest, ParseAndSetOptions_NoValue)
{
    bool savedDasm = Cbc::g_IsRawDisasmEnabled;
    const char* options[] = { "cbc.dasm=" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Cbc::g_IsRawDisasmEnabled, savedDasm);
}

TEST_F(OptionsTest, ParseAndSetOptions_MultipleKeysLastWins)
{
    const char* options[] = { "cbc.dasm=true", "cbc.dasm=false" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_FALSE(Cbc::g_IsRawDisasmEnabled);
}

TEST_F(OptionsTest, ParseAndSetOptions_AllLogLevels)
{
    const char* options[] = { "cbc.log.all=debug" };
    constexpr size_t optionsCount = ::std::size(options);

    Options::ParseAndSetOptions(static_cast<int>(optionsCount), options);

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::DEBUG);
    EXPECT_EQ(Interpretation::Log::interpretation.GetLogLevel(), Logging::Level::DEBUG);
    EXPECT_EQ(Interpretation::Log::preparation.GetLogLevel(), Logging::Level::DEBUG);
}

TEST_F(OptionsTest, InitEnvOptions)
{
    setenv("CBCOPT", "cbc.log.resolution=trace cbc.dasm=true cbc.path=/test/path", 1);

    Options::InitEnvOptions();

    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::TRACE);
    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(g_cbcPath, "/test/path");
}

TEST_F(OptionsTest, InitEnvOptions_NotSet)
{
    unsetenv("CBCOPT");

    Options::InitEnvOptions();

    EXPECT_EQ(Resolution::log.GetLogLevel(), saved.resLog);
}

TEST_F(OptionsTest, InitEnvOptions_Empty)
{
    setenv("CBCOPT", "", 1);

    Options::InitEnvOptions();

    EXPECT_EQ(Resolution::log.GetLogLevel(), saved.resLog);
}

TEST_F(OptionsTest, InitEnvOptions_LeadingTrailingSpaces)
{
    setenv("CBCOPT", "  cbc.dasm=true  ", 1);

    Options::InitEnvOptions();

    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
}

TEST_F(OptionsTest, InitEnvOptions_ConsecutiveSpaces)
{
    setenv("CBCOPT", "cbc.dasm=true  cbc.log.resolution=warn", 1);

    Options::InitEnvOptions();

    EXPECT_TRUE(Cbc::g_IsRawDisasmEnabled);
    EXPECT_EQ(Resolution::log.GetLogLevel(), Logging::Level::WARN);
}
