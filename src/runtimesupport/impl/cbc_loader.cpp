#include "cbc_loader.h"

#include <cerrno>
#include <dirent.h>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <utility>

#include "engine/engine.h"
#include "engine/symlevel/io/filesystem.h"
#include "utils/rt_logger.h"

namespace RTSupport {
namespace {

struct DirectoryCloser {
    void operator()(DIR* directory) const
    {
        if (directory != nullptr) {
            closedir(directory);
        }
    }
};

static void LogCbcDirectoryScan(std::string const& message)
{
    Log::rt.Log(Logging::Level::TRACE, [&message](Stream::Output& out) { out << message << Stream::endl; });
}

} // namespace

void LoadCbcFilesFromDirectory(Engine::Loader& loader, std::string const& cbcDir)
{
    errno = 0;
    std::unique_ptr<DIR, DirectoryCloser> directory(opendir(cbcDir.c_str()));
    if (directory == nullptr) {
        auto const message =
            errno == ENOENT || errno == ENOTDIR ? "cbc directory does not exist: " : "failed to access cbc directory: ";
        LogCbcDirectoryScan(message + cbcDir);
        return;
    }

    constexpr std::string_view cbcExtension = ".cbc";
    bool foundCbc                           = false;
    while (true) {
        errno       = 0;
        auto* entry = readdir(directory.get());
        if (entry == nullptr) {
            if (errno != 0) {
                LogCbcDirectoryScan("failed to scan cbc directory: " + cbcDir);
                return;
            }
            break;
        }

        std::string_view fileName(entry->d_name);
        if (fileName.size() <= cbcExtension.size()) {
            continue;
        }
        if (fileName.compare(fileName.size() - cbcExtension.size(), cbcExtension.size(), cbcExtension) != 0) {
            continue;
        }

        auto candidate = cbcDir;
        if (!candidate.empty() && candidate.back() != '/') {
            candidate += '/';
        }
        candidate += fileName;

        struct stat fileStat {};
        if (stat(candidate.c_str(), &fileStat) != 0 || !S_ISREG(fileStat.st_mode)) {
            continue;
        }

        foundCbc  = true;
        auto file = IO::TryOpenFile(candidate);
        if (!file.has_value()) {
            LogCbcDirectoryScan("failed to open cbc file: " + candidate);
            continue;
        }
        LogCbcDirectoryScan("successfully opened cbc file: " + candidate);
        loader.Load(std::move(file.value()), candidate);
    }

    if (!foundCbc) {
        LogCbcDirectoryScan("no .cbc files found in cbc directory: " + cbcDir);
    }
}

} // namespace RTSupport
