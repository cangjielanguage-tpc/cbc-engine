#pragma once

#include <string>

#include "utils/logger.h"
#include "utils/options.h"

namespace {

bool SetLogLevelValue(Options::Table const&, Options::Option const& option, std::string_view value)
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

bool SetBoolValue(Options::Table const&, Options::Option const& option, std::string_view value)
{
    if (value == "true" || value == "1") {
        *(bool*)option.location = true;
        return true;
    } else if (value == "false" || value == "0") {
        *(bool*)option.location = false;
        return true;
    } else {
        return false;
    }
}

bool SetStringValue(Options::Table const&, Options::Option const& option, std::string_view value)
{
    *(std::string*)(option.location) = value;
    return true;
}

bool SetAllLogLevels(Options::Table const& t, Options::Option const&, std::string_view value)
{
    for (size_t i = 0; i < t.Size(); ++i) {
        auto& opt = t[i];
        if (opt.setter == &SetAllLogLevels) {
            continue;
        }
        if (opt.setter == &SetLogLevelValue) {
            if (!SetLogLevelValue(t, opt, value)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace
