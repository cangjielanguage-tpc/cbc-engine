#include "utils/options.h"
#include "utils/options_setup.h"

#include "cbc/isa_disasm.h"
#include "interpreter/loggers.h"
#include "resolution/resolution.h"
#include "runtimesupport/impl/entrypoint.h"
#include "utils/ostream.h"

#include <cstdlib>
#include <string_view>
#include <vector>

namespace Options {

constexpr Option globalOptionsArray[] = {
    { "cbc.log.resolution", &Resolution::log, &SetLogLevelValue },
    { "cbc.log.int", &Interpretation::Log::interpretation, &SetLogLevelValue },
    { "cbc.log.preparation", &Interpretation::Log::preparation, &SetLogLevelValue },
    { "cbc.log.all", nullptr, &SetAllLogLevels },
    { "cbc.dasm", &Cbc::g_IsRawDisasmEnabled, &SetBoolValue },
    { "cbc.path", &g_cbcPath, &SetStringValue },
    { "cbc.main", &g_mainCbc, &SetStringValue },
};

Table const g_table(globalOptionsArray);

Stream::Descripted warn(Stream::cerr, "[WARNING] ");

void PrintError(std::string_view prefix, std::string_view str) { warn << prefix << " " << str << Stream::endl; };

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

Table::Snapshot Table::SaveContext() const
{
    Snapshot snap;
    for (size_t i = 0; i < size_; ++i) {
        auto& opt = options_[i];
        if (opt.setter == &SetLogLevelValue) {
            auto level = reinterpret_cast<Logging::Logger*>(opt.location)->GetLogLevel();
            snap.entries.push_back({ i, std::string(1, static_cast<char>(level)) });
        } else if (opt.setter == &SetBoolValue) {
            snap.entries.push_back({ i, std::string(1, *(bool*)opt.location ? 1 : 0) });
        } else if (opt.setter == &SetStringValue) {
            snap.entries.push_back({ i, *(std::string*)opt.location });
        }
    }
    return snap;
}

void Table::RestoreContext(Snapshot const& snap) const
{
    for (auto& entry : snap.entries) {
        auto& opt = options_[entry.index];
        if (opt.setter == &SetLogLevelValue) {
            auto level = static_cast<Logging::Level>(entry.data[0]);
            reinterpret_cast<Logging::Logger*>(opt.location)->SetLogLevel(level);
        } else if (opt.setter == &SetBoolValue) {
            *(bool*)opt.location = entry.data[0] != 0;
        } else if (opt.setter == &SetStringValue) {
            *(std::string*)opt.location = entry.data;
        }
    }
}

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

void SetOptions(std::vector<KeyVal> const& parsedOpts, Table const& opts)
{
    for (auto& parsedOpt : parsedOpts) {
        switch (opts.Set(parsedOpt.key, parsedOpt.val)) {
            case Table::Status::INVALID_OPTION: PrintError("invalid option", parsedOpt.whole); break;
            case Table::Status::UNKNOWN_OPTION: PrintError("unknown option", parsedOpt.whole); break;
            default:                            continue;
        }
    }
}

void ParseAndSet(int size, char const** optStr, Table const& opts)
{
    if (optStr == nullptr) {
        return;
    }

    std::vector<KeyVal> parsedOpts;
    for (size_t i = 0; i < size; ++i) {
        ParseKeyVal(parsedOpts, optStr[i]);
    }

    SetOptions(parsedOpts, opts);
}

void InitFromString(std::string_view optStr, Table const& opts)
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

void InitFromEnv(Table const& opts)
{
    auto _optStr = std::getenv("CBCOPT");
    if (_optStr == nullptr) {
        return;
    }

    InitFromString(std::string_view(_optStr), opts);
}

} // namespace Options
