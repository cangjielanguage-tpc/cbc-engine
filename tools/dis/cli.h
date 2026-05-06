#pragma once

#include <optional>

namespace Dis {
namespace Cli {

class CliOptions {
    char** current;
    char** end;
    char* executable;

    void parse();

public:
    CliOptions(int argc, char** argv);

    std::optional<char*> next();
};
} // namespace Cli
} // namespace Dis
