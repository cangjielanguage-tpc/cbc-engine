#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace Dis {
namespace Env {

struct Option {
    const std::string name;
};

#define OPTIONS_AMOUNT 2

inline BoolOption resolve = BoolOption { "r", "resolve", false };

inline ValueOption cbcPath = ValueOption { "cp", "cbc", "" };

using Callback = std::function<void(Option&, std::string)>;

inline std::unordered_map<std::string, Option&, Callback> options = { { resolve.shortName, resolve },
                                                                      { cbcPath.shortName, cbcPath } };

} // namespace Env
} // namespace Dis
