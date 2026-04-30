#include "cli_parser.h"
#include "disasmer.h"
#include "utils/ostream.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Cli {

using namespace std;

void CliParser::Help()
{
    cerr << "Cbc disassembler" << endl;

    cerr << endl;

    cerr << "USAGE:" << endl;
    cerr << "  disasm [OPTIONS] <FILE...>" << endl;
    cerr << "  disasm <FILE...> [OPTIONS]" << endl;

    cerr << endl;

    cerr << "OPTIONS:" << endl;
    cerr << "  -h, --help    show this message and exit" << endl;
    cerr << "  --no-resolve  don't resolve offsets; print raw offsets instead" << endl;

    cerr << endl;

    cerr << "EXAMPLES:" << endl;
    cerr << "  disasm --no-resolve file.cbc" << endl;
    cerr << "  disasm file1.cbc file2.cbc file3.cbc" << endl;
}

bool CliParser::isOption(string_view sv)
{
    return sv.size() >= 2 && (sv.substr(0, 2) == "--" || sv.substr(0, 1) == "-");
}

void CliParser::ParseOption(string_view sv, DisasmerBuilder& builder)
{
    auto it = opts.find(sv);
    if (it == opts.end()) {
        cerr << "Incorrect option: " << sv << endl;
        cerr << endl;
        Help();
        exit(1);
    }

    it->second(builder);
}

Dis::Disasmer CliParser::CreateDisasmer(Stream::Output& out)

{
    auto builder = DisasmerBuilder(out, args.size());
    for (auto arg : args) {
        if (isOption(arg)) {
            ParseOption(arg, builder);
        } else {
            filesystem::path p(arg);
            assert(filesystem::exists(p));

            builder.AddFile(arg);
        }
    }

    return builder.Build();
}
} // namespace Cli
