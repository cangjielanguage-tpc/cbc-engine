#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "cbc/isa_disasm.h"
#include "runtimesupport/impl/entrypoint.h"

#include <charconv>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace Options {

Stream::Descripted warn(Stream::cerr, "[WARNING] ");

struct Option;

using OptionSetter = bool (*)(Option const&, std::string_view value);

struct Option {
    std::string_view name;
    void* location;
    OptionSetter setter;
};

static void InvalidOptionWarning(Option const& option, std::string_view value)
{
    warn.PrintFmt("invalid option %.*s=%.*s", option.name.size(), option.name.data(), value.size(), value.data());
    warn.NewLine();
}

template <typename Num> static bool SetNumOption(Option const& option, std::string_view value)
{
    Num val;
    auto res = std::from_chars(value.begin(), value.end(), val);
    if (res.ptr == value.end()) {
        *reinterpret_cast<int*>(option.location) = val;
        return true;
    }
    return false;
}

static bool SetLogLevelOption(Option const& option, std::string_view value)
{
    auto loc = reinterpret_cast<Logging::Logger*>(option.location);

    struct Matcher {
        char const* str;
        Logging::Level level;
    };

    Matcher matchers[] = {
        { "none", Logging::Level::NONE },   { "fatal", Logging::Level::FATAL }, { "error", Logging::Level::ERROR },
        { "warn", Logging::Level::WARN },   { "info", Logging::Level::INFO },   { "debug", Logging::Level::DEBUG },
        { "trace", Logging::Level::TRACE },
    };

    for (auto& m : matchers) {
        if (value.compare(m.str) == 0) {
            loc->SetLogLevel(m.level);
            return true;
        }
    }

    return false;
}

static bool SetLogLevelOptionForAll(Option const& option, std::string_view value);
static bool SetBoolOption(const Option& option, std::string_view value);
static bool SetStrViewOption(const Option& option, std::string_view value);

constexpr Option options[] = {
    { "cbc.log.resolution", &Resolution::log, &SetLogLevelOption },
    { "cbc.log.int", &Interpretation::Log::interpretation, &SetLogLevelOption },
    { "cbc.log.preparation", &Interpretation::Log::preparation, &SetLogLevelOption },
    { "cbc.log.all", nullptr, &SetLogLevelOptionForAll },
    { "cbc.dasm", &Cbc::g_IsRawDisasmEnabled, &SetBoolOption },
    { "cbc.path", &g_cbcPath, &SetStrViewOption },
    { "cbc.main", &g_mainCbc, &SetStrViewOption }
};

static bool SetLogLevelOptionForAll(Option const& option, std::string_view value)
{
    for (auto& opt : options) {
        if (opt.setter != &SetLogLevelOption) {
            continue;
        }
        if (!SetLogLevelOption(opt, value)) {
            return false;
        }
    }
    return true;
}

static bool SetBoolOption(const Option& option, std::string_view value)
{
    if (value == "true") {
        *(bool*)options->location = true;
        return true;
    } else if (value == "false") {
        *(bool*)options->location = false;
        return true;
    } else {
        return false;
    }
}

static bool SetStrViewOption(const Option& option, std::string_view value)
{
    *(std::string_view*)(option.location) = value;
    return true;
}

enum class SetOptionStatus {
    OK,
    UNKNOWN_OPTION,
    INVALID_OPTION
};

SetOptionStatus SetOption(std::string_view parsedOpt, std::string_view parsedVal)
{
    for (auto& opt : options) {
        if (parsedOpt.compare(opt.name) == 0) {
            if (!opt.setter(opt, parsedVal)) {
                return SetOptionStatus::INVALID_OPTION;
            }
            return SetOptionStatus::OK;
        }
    }
    return SetOptionStatus::UNKNOWN_OPTION;
}

struct KeyVal {
    std::string_view key;
    std::string_view val;
    std::string_view whole;
};

void PrintError(std::string_view prefix, std::string_view str)
{
    warn.PrintFmt("%.*s %.*s", prefix.size(), prefix.data(), str.size(), str.data());
    warn.NewLine();
};

void ParseKeyVal(std::vector<KeyVal>& parsedOpts, std::string_view kv)
{
    size_t eqPos = kv.find('=');
    if (eqPos != std::string::npos && eqPos + 1 < kv.size()) {
        auto key = kv.substr(0, eqPos);
        auto val = kv.substr(eqPos + 1);
        parsedOpts.emplace_back(KeyVal { key, val, kv });
    } else {
        PrintError("invalid option format", kv);
    }
};

void SetOptions(const std::vector<KeyVal>& parsedOpts)
{
    for (auto& parsedOpt : parsedOpts) {
        switch (SetOption(parsedOpt.key, parsedOpt.val)) {
            case SetOptionStatus::INVALID_OPTION: PrintError("invalid option", parsedOpt.whole); break;
            case SetOptionStatus::UNKNOWN_OPTION: PrintError("unknown option", parsedOpt.whole); break;
            default: continue;
        }
    }
}

void ParseAndSetOptions(int size, char const** _optStr)
{
    if (_optStr == nullptr) {
        return;
    }

    std::vector<KeyVal> parsedOpts;
    for (size_t i {0}; i < size; ++i) {
        PrintError("Parsing:", _optStr[i]);
        ParseKeyVal(parsedOpts, _optStr[i]);
    }
    for (auto& x : parsedOpts) {
        PrintError("Parsed key:", x.key);
        PrintError("Parsed val:", x.val);
        PrintError("Parsed whole:", x.whole);
    }

    SetOptions(parsedOpts);
}


void InitEnvOptions()
{
    // TODO: implement properly

    auto _optStr = std::getenv("CBCOPT");
    if (_optStr == nullptr) {
        return;
    }

    std::string_view optStr(_optStr);
    std::vector<KeyVal> parsedOpts;

    size_t start = 0;
    size_t end = 0;
    while ((end = optStr.find(' ', start)) != std::string_view::npos) {
        if (end > start) {
            ParseKeyVal(parsedOpts, optStr.substr(start, end - start));
        }
        start = end + 1;
    }

    if (start < optStr.size()) {
        ParseKeyVal(parsedOpts, optStr.substr(start));
    }

    SetOptions(parsedOpts);
}

} // namespace Options
