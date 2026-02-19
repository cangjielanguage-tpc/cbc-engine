#include "filesystem.h"
#include "byte_array_random_access_file.h"

namespace IO {

RandomAccessFile* OpenFile(std::filesystem::path path)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        throw std::runtime_error("cannot open file " + path.string());
    }

    fseek(file, 0, SEEK_END);
    long rawLength = ftell(file);
    if (rawLength <= 0) {
        fclose(file);
        throw std::runtime_error("empty file " + path.string());
    }

    rewind(file);

    size_t fileLength = static_cast<size_t>(rawLength);

    char* data       = new char[fileLength];
    size_t bytesRead = fread(data, 1, fileLength, file);

    fclose(file);

    if (bytesRead != fileLength) {
        delete[] data;
        throw std::runtime_error("read error " + path.string());
    }

    return new ByteArrayRandomAccessFile(data, fileLength);
};

} // namespace IO
