#include "cli.h"
// #include "engine/symlevel/cbc_file.h"
// #include "engine/symlevel/io/filesystem.h"
// #include "engine/symlevel/terms.h"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
    auto opts = Dis::Cli::CliOptions(argc, argv);
    // std::string name = "simple.asm";
    // auto raf         = IO::OpenFile(std::filesystem::path(name));
    // auto cbc         = Symlevel::CbcFile::Create(IO::FileId(0), *raf, name);
    // std::cout << cbc.GetPath() << std::endl;
}
