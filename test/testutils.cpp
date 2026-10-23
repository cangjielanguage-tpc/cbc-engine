#include "testutils.h"
#include "engine/image/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

static std::string AsmJarPath() { return std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar"; }

bool CheckForAssembler() { return IO::TryOpenFile(AsmJarPath()).has_value(); }

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

static void RunAssembler(std::string const& asm_path)
{
    auto command = "java -jar " + AsmJarPath() + ' ' + asm_path + " 2>&1";

    std::string output;
    auto pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        FATAL("cannot run %s", command.c_str());
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }
    auto status = pclose(pipe);
    if (status != 0) {
        FATAL("assembler failed with rc=%d: %s\n%s", status, command.c_str(), output.c_str());
    }
}

static std::unique_ptr<IO::RandomAccessFile> OpenCbc(std::string const& cbc_path)
{
    auto file = IO::OpenFile(cbc_path);
    if (!file.has_value()) {
        FATAL("assembler produced no %s", cbc_path.c_str());
    }
    return std::move(file.value());
}

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = std::string(TEST_RESOURCE_DIR) + "/" + file_name;

    RunAssembler(asm_path);

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
    return OpenCbc(target_path);
}

std::unique_ptr<IO::RandomAccessFile> OpenAsmText(const std::string& name, std::string_view asm_text)
{
    if (!CheckForAssembler()) {
        FATAL("assembler is not available");
    }

    auto asm_path = std::string(TEST_AUTOGEN_DIR) + "/" + CurrentTestDir() + "/" + name;
    CreateDirectories(DirName(asm_path));

    auto out = fopen(asm_path.c_str(), "wb");
    if (out == nullptr) {
        FATAL("cannot write %s", asm_path.c_str());
    }
    fwrite(asm_text.data(), 1, asm_text.size(), out);
    fclose(out);

    RunAssembler(asm_path);
    return OpenCbc(AsmToCbc(asm_path));
}
