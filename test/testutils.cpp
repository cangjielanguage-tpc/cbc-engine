#include "testutils.h"
#include "engine/symlevel/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>

bool CheckForAssembler()
{
    auto jar_path = std::string(TEST_RESOURCE_DIR).append("/").append("cbc-asm.jar");
    return std::filesystem::exists(jar_path);
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string_view file_name)
{
    auto asm_path = std::string(TEST_RESOURCE_DIR).append("/").append(file_name);
    auto cbc_path = std::string(TEST_RESOURCE_DIR).append("/").append(file_name).append(".obj");
    auto jar_path = std::string(TEST_RESOURCE_DIR).append("/").append("cbc-asm.jar");

    auto command = std::string("java -jar ").append(jar_path).append(" ").append(asm_path);
    auto file    = popen(command.c_str(), "r");
    pclose(file);
    return IO::OpenFile(std::filesystem::path(cbc_path));
}
