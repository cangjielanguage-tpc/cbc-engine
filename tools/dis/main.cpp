#include "cli_parser.h"

using OptionsMap =
    std::unordered_map<std::string_view, Utils::Function<void(Cli::CliParser&, Cli::CliParser::DisasmerBuilder&)>>;

static auto help = [](Cli::CliParser& cli, auto&) {
    cli.Help();
    exit(0);
    //
};

static auto noResolve = [](auto&, Cli::CliParser::DisasmerBuilder& builder) { builder.SetResolving(false); };

OptionsMap opts = { { "-h", help }, { "--help", help }, { "--no-resolve", noResolve } };

int main(int argc, char* argv[]) { Cli::CliParser(argc, argv, opts).CreateDisasmer(Stream::cout).Disasm(); }
