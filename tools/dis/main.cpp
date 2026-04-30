#include "cli_parser.h"

int main(int argc, char* argv[]) { Cli::CliParser(argc, argv).CreateDisasmer(Stream::cout).Disasm(); }
