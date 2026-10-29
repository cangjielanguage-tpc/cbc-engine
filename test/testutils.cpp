#include "testutils.h"
#include "engine/image/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static std::string ResourcePath(std::string const& name)
{
    return std::string(TEST_RESOURCE_DIR) + "/" + name;
}

static bool IsExecutable(std::string const& path)
{
    return ::access(path.c_str(), X_OK) == 0;
}

bool CheckForAssembler()
{
    return IsExecutable(ResourcePath("cbc-asm"));
}

static bool ends_with(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

static std::string AsmToCbc(std::string const& path)
{
    std::string result = path;
    if (ends_with(result, ".asm")) {
        result.resize(result.size() - 4);
    }
    return result + ".cbc";
}

static std::string_view DirName(std::string_view path)
{
    auto pos = path.rfind('/');
    return pos == std::string_view::npos ? std::string_view() : path.substr(0, pos);
}

static void CreateDirectories(std::string_view path)
{
    std::string::size_type pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string_view::npos) {
        mkdir(std::string(path.substr(0, pos)).c_str(), 0755);
    }
    mkdir(std::string(path).c_str(), 0755);
}

// Directory (relative to TEST_AUTOGEN_DIR) holding the .cbc files produced for the
// currently running test. Parameterized test names may contain '/', which is not
// usable as a single path component.
static std::string CurrentTestDir()
{
    auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    if (info == nullptr) {
        return "standalone";
    }
    std::string name = std::string(info->test_suite_name()) + "." + info->name();
    std::replace(name.begin(), name.end(), '/', '_');
    return name;
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = ResourcePath(file_name);

    auto command = ResourcePath("cbc-asm") + ' ' + asm_path;
    auto file    = popen(command.c_str(), "r");
    pclose(file);

    // The assembler always emits the .cbc next to the .asm. The resources directory
    // must stay free of generated files, so relocate the fresh .cbc into a
    // test-specific directory under TEST_AUTOGEN_DIR and load it from there.
    auto rel_name = AsmToCbc(file_name);
    if (rel_name.rfind("./", 0) == 0) {
        rel_name = rel_name.substr(2);
    }

    auto target_dir  = std::string(TEST_AUTOGEN_DIR) + "/" + CurrentTestDir();
    auto target_path = target_dir + "/" + rel_name;
    CreateDirectories(DirName(target_path));

    auto generated_path = AsmToCbc(asm_path);
    if (std::rename(generated_path.c_str(), target_path.c_str()) != 0) {
        FATAL("cannot move %s to %s", generated_path.c_str(), target_path.c_str());
    }
    return std::move(IO::OpenFile(target_path).value());
}
