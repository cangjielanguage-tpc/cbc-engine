#pragma once

#include "disasmer.h"
#include "utils/ostream.h"
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Cli {
using std::string_view;

struct CliParser {
public:
    class DisasmerBuilder {
        Utils::Vector<string_view> files;

        Stream::Output& out;

        bool resolving = true;

    public:
        void SetResolving(bool resolving) { this->resolving = resolving; }

        void AddFile(string_view file) { files.push_back(file); }

        DisasmerBuilder(Stream::Output& out, size_t expectedSize) : out(out) { files.reserve(expectedSize); }

        Dis::Disasmer Build() { return Dis::Disasmer(files, out, resolving); }
    };

    using OptionsMap = std::unordered_map<string_view, std::function<void(CliParser&, DisasmerBuilder&)>>;

    CliParser(int argc, char* argv[], OptionsMap options);

    Dis::Disasmer CreateDisasmer(Stream::Output& out);

    void Help();

private:
    OptionsMap opts;

    Utils::Vector<string_view> args;

    bool isOption(string_view sv);

    void ParseOption(string_view sv, DisasmerBuilder& builder);
};
} // namespace Cli
