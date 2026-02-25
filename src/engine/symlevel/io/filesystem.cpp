#include "filesystem.h"
#include "byte_array_random_access_file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace IO {

std::unique_ptr<RandomAccessFile> OpenFile(std::filesystem::path path)
{
    // FIXME: remove linux specific code
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("cannot open file " + path.string());
    }

    struct stat stat;

    int status     = fstat(fd, &stat);
    auto rawLength = stat.st_size; // TODO: not working with symbolic links
    if (rawLength <= 0) {
        close(fd);
        throw std::runtime_error("empty file " + path.string());
    }

    size_t fileLength = static_cast<size_t>(rawLength);

    char* data       = new char[fileLength];
    ssize_t n        = read(fd, data, fileLength);

    close(fd);

    if (n != fileLength) {
        delete[] data;
        throw std::runtime_error("read error " + path.string());
    }

    return std::make_unique<ByteArrayRandomAccessFile>(data, fileLength);
};

} // namespace IO
