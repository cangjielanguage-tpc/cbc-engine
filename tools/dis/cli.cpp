#include "cli.h"
#include <cassert>
#include <optional>
#include <string.h>

namespace Dis {
namespace Cli {

CliOptions::CliOptions(int argc, char** argv) : executable(argv[0]), current(&argv[1]), end(&argv[argc]) { parse(); }

std::optional<char*> CliOptions::next()
{
    assert(this->current <= this->end);
    if (this->current == this->end) {
        return std::nullopt;
    }

    auto ret = std::optional { *this->current };
    ++this->current;
    return ret;
}

bool isLongOption(char* option) { return strncmp(const_cast<char*>("--"), option, 2) == 0; }

bool isShortOption(char* option) { return !isLongOption(option) && (strncmp(const_cast<char*>("-"), option, 1) == 0); }

bool isValue(char* option) { return !isLongOption(option) && !isShortOption(option); }

void parseLongOption(char* option) {}

void CliOptions::parse()
{
    while (true) {
        auto opt = next();
        if (!opt.has_value()) {
            break;
        }
        auto option = *opt;

        if (isValue(option)) {
            // parse value. add cbc file to queue or something like that, idk
            continue;
        }

        if (isShortOption(option)) {
            // parse short option, that usually means taking the next stream token
            continue;
        }

        if (isLongOption(option)) {
            // parse long option
            continue;
        }
    }
}

} // namespace Cli
} // namespace Dis
