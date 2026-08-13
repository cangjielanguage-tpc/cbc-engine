#include "testutils.h"
#include "engine/symlevel/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <memory>
#include <optional>

bool CheckForAssembler()
{
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";
    return IO::TryOpenFile(jar_path).has_value();
}

static bool ends_with(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name;
    auto cbc_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name;
    if (ends_with(cbc_path, ".asm")) {
        cbc_path = cbc_path.substr(0, cbc_path.size() - 4);
    }
    cbc_path = cbc_path + ".cbc";
    auto jar_path = std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar";

    auto command = "java -jar " + jar_path + ' ' + asm_path;
    auto file    = popen(command.c_str(), "r");
    pclose(file);
    return std::move(IO::OpenFile(cbc_path).value());
}
