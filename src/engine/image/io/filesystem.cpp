#include "filesystem.h"
#include "byte_array_random_access_file.h"
#include "utils/rt_logger.h"

#include <fcntl.h>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>

namespace IO {

static std::optional<std::unique_ptr<RandomAccessFile>> OpenFileImpl(std::string const& path, bool log_failure)
{
    // FIXME: remove linux specific code
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        if (log_failure) {
            RTSupport::Log::gc.Log(Logging::Level::ERROR, [&path](Stream::Output& out) {
                out.PrintFmtLn("cannot open file %s", path.c_str());
            });
        }
        return std::nullopt;
    }

    struct stat stat;

    int status     = fstat(fd, &stat);
    auto rawLength = stat.st_size; // TODO: not working with symbolic links
    if (rawLength <= 0) {
        close(fd);
        if (log_failure) {
            RTSupport::Log::gc.Log(Logging::Level::ERROR, [&path](Stream::Output& out) {
                out.PrintFmtLn("empty file %s", path.c_str());
            });
        }
        return std::nullopt;
    }

    size_t fileLength = static_cast<size_t>(rawLength);

    char* data = new char[fileLength];
    ssize_t n  = read(fd, data, fileLength);

    close(fd);

    if (n != fileLength) {
        delete[] data;
        if (log_failure) {
            RTSupport::Log::gc.Log(Logging::Level::ERROR, [&path](Stream::Output& out) {
                out.PrintFmtLn("read error %s", path.c_str());
            });
        }
        return std::nullopt;
    }

    return std::make_unique<ByteArrayRandomAccessFile>(data, fileLength);
};

std::optional<std::unique_ptr<RandomAccessFile>> OpenFile(std::string const& path) { return OpenFileImpl(path, true); }

std::optional<std::unique_ptr<RandomAccessFile>> TryOpenFile(std::string const& path)
{
    return OpenFileImpl(path, false);
}

} // namespace IO
