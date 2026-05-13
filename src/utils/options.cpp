#include "utils/options.h"
#include "engine/options.h"
#include "utils/logger.h"
#include "utils/ostream.h"

#include <cstdlib>
#include <string_view>
#include <vector>

namespace Options {

Stream::Descripted warn(Stream::cerr, "[WARNING] ");

void PrintError(std::string_view prefix, std::string_view str) { warn << prefix << " " << str << Stream::endl; };

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

bool SetAllLogLevels(Table const& t, Option const&, std::string_view value)
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

Table::Status Table::Set(std::string_view key, std::string_view value) const
{
    for (size_t i = 0; i < size_; ++i) {
        if (options_[i].name == key) {
            if (!options_[i].setter(*this, options_[i], value)) {
                return Status::INVALID_OPTION;
            }
            return Status::OK;
        }
    }
    return Status::UNKNOWN_OPTION;
}

namespace {

struct KeyVal {
    std::string_view key;
    std::string_view val;
    std::string_view whole;
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

void SetOptions(const std::vector<KeyVal>& parsedOpts, const Table& opts)
{
    for (auto& parsedOpt : parsedOpts) {
        switch (opts.Set(parsedOpt.key, parsedOpt.val)) {
            case Table::Status::INVALID_OPTION: PrintError("invalid option", parsedOpt.whole); break;
            case Table::Status::UNKNOWN_OPTION: PrintError("unknown option", parsedOpt.whole); break;
            default:                            continue;
        }
    }
}

} // namespace

void Table::ParseAndSet(int size, const char* const* optStr) const
{
    if (optStr == nullptr) {
        return;
    }

    std::vector<KeyVal> parsedOpts;
    for (size_t i = 0; i < size; ++i) {
        ParseKeyVal(parsedOpts, optStr[i]);
    }

    SetOptions(parsedOpts, *this);
}

void InitFromString(std::string_view optStr, const Table& opts)
{
    std::vector<KeyVal> parsedOpts;

    size_t start = 0;
    size_t end   = 0;
    while ((end = optStr.find(' ', start)) != std::string_view::npos) {
        if (end > start) {
            ParseKeyVal(parsedOpts, optStr.substr(start, end - start));
        }
        start = end + 1;
    }

    if (start < optStr.size()) {
        ParseKeyVal(parsedOpts, optStr.substr(start));
    }

    SetOptions(parsedOpts, opts);
}

void InitFromEnv(const Table& opts)
{
    auto _optStr = std::getenv("CBCOPT");
    if (_optStr == nullptr) {
        return;
    }

    InitFromString(std::string_view(_optStr), opts);
}

} // namespace Options
