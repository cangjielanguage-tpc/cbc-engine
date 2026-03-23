#include "testutils.h"
#include "engine/symlevel/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>

bool CheckForAssembler()
{
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";
    return std::filesystem::exists(jar_path);
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name;
    auto cbc_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name + ".obj";
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";

    auto command = "java -jar " + jar_path + ' ' + asm_path;
    auto file    = popen(command.c_str(), "r");
    pclose(file);
    return IO::OpenFile(std::filesystem::path(cbc_path));
}
