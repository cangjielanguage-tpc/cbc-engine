#pragma once

#include "disasmer.h"
#include "utils/ostream.h"
#include <string_view>
#include <vector>

namespace Cli {
using namespace std;

class CliParser {
    vector<string_view> args;

    bool isOption(string_view sv);

    void Help();

    class DisasmerBuilder {
        vector<string_view> files;

        Stream::Output& out;

        bool resolving = true;

    public:
        void SetResolving(bool resolving) { this->resolving = resolving; }

        void AddFile(string_view file) { files.push_back(file); }

        DisasmerBuilder(Stream::Output& out, size_t expectedSize) : out(out) { files.reserve(expectedSize); }

        Dis::Disasmer Build() { return Dis::Disasmer(files, out, resolving); }
    };

    void ParseOption(string_view sv, DisasmerBuilder& builder);

    std::unordered_map<string_view, std::function<void(DisasmerBuilder&)>> opts = {
        { "-h",
          [&](auto&) {
              Help();
              exit(0);
              //
          } },
        { "--help",
          [&](auto&) {
              Help();
              exit(0);
          } },
        { "--no-resolve", [&](DisasmerBuilder& builder) { builder.SetResolving(false); } }
    };

public:
    CliParser(int argc, char* argv[])
    {
        assert(argc > 0);
        args.reserve(argc);
        for (size_t i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }
    }

    Dis::Disasmer CreateDisasmer(Stream::Output& out);
};
} // namespace Cli
