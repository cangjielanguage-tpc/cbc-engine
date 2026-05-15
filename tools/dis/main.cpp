#include "cli_parser.h"

using OptionsMap =
    std::unordered_map<std::string_view, std::function<void(Cli::CliParser&, Cli::CliParser::DisasmerBuilder&)>>;

OptionsMap opts = { { "-h",
                      [](auto& cli, auto&) {
                          cli.Help();
                          exit(0);
                          //
                      } },
                    { "--help",
                      [](auto& cli, auto&) {
                          cli.Help();
                          exit(0);
                      } },
                    { "--no-resolve",
                      [](auto&, Cli::CliParser::DisasmerBuilder& builder) { builder.SetResolving(false); } } };

int main(int argc, char* argv[]) { Cli::CliParser(argc, argv, opts).CreateDisasmer(Stream::cout).Disasm(); }
