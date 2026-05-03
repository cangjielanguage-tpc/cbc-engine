#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "utils/logger.h"
#include "utils/ostream.h"
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

constexpr Option options[] = {
    { "cbc.log.resolution", &Resolution::log, &SetLogLevelOption },
    { "cbc.log.int", &Interpretation::Log::interpretation, &SetLogLevelOption },
    { "cbc.log.preparation", &Interpretation::Log::preparation, &SetLogLevelOption },
};

void InitEnvOptions()
{
    // TODO: implement properly

    auto _optStr = std::getenv("CBCOPT");
    if (_optStr == nullptr) {
        return;
    }

    struct KeyVal {
        std::string_view key;
        std::string_view val;
        std::string_view whole;
    };

    auto optStr = std::string_view(_optStr);
    std::vector<KeyVal> parsedOpts;

    auto error = [](std::string_view prefix, std::string_view str) {
        warn.PrintFmt("%.*s %.*s", prefix.size(), prefix.data(), str.size(), str.data());
        warn.NewLine();
    };

    auto parseKeyVal = [&parsedOpts, &error](std::string_view kv) {
        size_t eqPos = kv.find('=');
        if (eqPos != std::string::npos && eqPos + 1 < kv.size()) {
            auto key = kv.substr(0, eqPos);
            auto val = kv.substr(eqPos + 1);
            parsedOpts.emplace_back(KeyVal { key, val, kv });
        } else {
            error("unknown option", kv);
        }
    };

    size_t pos  = 0;
    size_t prev = 0;
    while ((pos = optStr.find(' ', prev)) != std::string::npos) {
        parseKeyVal(optStr.substr(prev, pos - prev));
        prev = pos + 1;
    }
    parseKeyVal(optStr.substr(prev));

    for (auto& parsedOpt : parsedOpts) {
        bool found = false;
        for (auto& opt : options) {
            if (parsedOpt.key.compare(opt.name) == 0) {
                found = true;
                if (!opt.setter(opt, parsedOpt.val)) {
                    error("invalid option", parsedOpt.whole);
                }
                break;
            }
        }
        if (!found) {
            error("unknown option", parsedOpt.whole);
        }
    }
}

} // namespace Options
