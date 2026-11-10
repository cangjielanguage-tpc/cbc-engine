#include "testutils.h"
#include "engine/image/io/filesystem.h"
#include "stdio.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <sys/file.h>
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

// Inter-process lock guarding the assemble-and-move sequence in OpenAsm: the
// external assembler writes its .cbc next to the .asm source, and parallel ctest
// processes may assemble the same .asm concurrently — one process would then
// rename the shared .cbc away while another is still writing or about to move
// it. One lock file per .asm under TEST_AUTOGEN_DIR (gitignored); flock is
// released when the descriptor is closed.
class AsmFileLock {
public:
    explicit AsmFileLock(const std::string& asmPath)
    {
        auto lockDir = std::string(TEST_AUTOGEN_DIR) + "/asm-locks";
        CreateDirectories(lockDir);
        auto lockName = asmPath;
        std::replace(lockName.begin(), lockName.end(), '/', '_');
        fd = open((lockDir + "/" + lockName + ".lock").c_str(), O_CREAT | O_RDWR, 0644);
        flock(fd, LOCK_EX);
    }

    ~AsmFileLock() { close(fd); }

private:
    int fd = -1;
};

std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name)
{
    auto asm_path = ResourcePath(file_name);

    // The assembler always emits the .cbc next to the .asm. The resources directory
    // must stay free of generated files, so relocate the fresh .cbc into a
    // test-specific directory under TEST_AUTOGEN_DIR and load it from there.
    auto rel_name = AsmToCbc(file_name);
    if (rel_name.rfind("./", 0) == 0) {
        rel_name = rel_name.substr(2);
    }

    auto target_dir  = std::string(TEST_AUTOGEN_DIR) + "/" + CurrentTestDir();
    auto target_path = target_dir + "/" + rel_name;

    // Assembly is deterministic given the .asm source, so an existing .cbc serves
    // as an on-filesystem cache. It is reassembled when missing or older than the
    // source (e.g. after the .asm files are regenerated or checked out anew).
    struct stat asmStat = {};
    struct stat cbcStat = {};
    if (::stat(target_path.c_str(), &cbcStat) == 0 && ::stat(asm_path.c_str(), &asmStat) == 0 &&
        asmStat.st_mtime <= cbcStat.st_mtime) {
        return std::move(IO::OpenFile(target_path).value());
    }

    CreateDirectories(DirName(target_path));

    AsmFileLock asmLock(asm_path);
    auto command = ResourcePath("cbc-asm") + ' ' + asm_path;
    auto file    = popen(command.c_str(), "r");
    pclose(file);

    auto generated_path = AsmToCbc(asm_path);
    if (std::rename(generated_path.c_str(), target_path.c_str()) != 0) {
        FATAL("cannot move %s to %s", generated_path.c_str(), target_path.c_str());
    }
    return std::move(IO::OpenFile(target_path).value());
}
