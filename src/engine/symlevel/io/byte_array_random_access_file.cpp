#include <cstring>

#include "byte_array_random_access_file.h"

namespace IO {

size_t ByteArrayRandomAccessFile::Peek(char* array, size_t position, size_t length) const
{
    if (array == nullptr || position >= fileLength) {
        return 0;
    }

    size_t realEnd = std::min(position + length, fileLength);
    size_t realLen = realEnd - position;

    memcpy(array, data + position, realLen);

    return realLen;
};


std::tuple<char*, size_t> ByteArrayRandomAccessFile::OpenFile(std::filesystem::path path)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        throw std::runtime_error("cannot open cbc file " + path.string());
    }

    fseek(file, 0, SEEK_END);
    long rawLength = ftell(file);
    if (rawLength <= 0) {
        fclose(file);
        throw std::runtime_error("empty cbc file " + path.string());
    }

    rewind(file);

    size_t fileLength = static_cast<size_t>(rawLength);

    char* data = new char[fileLength];
    size_t bytesRead = fread(data, 1, fileLength, file);

    fclose(file);

    if (bytesRead != fileLength) {
        delete[] data;
        throw std::runtime_error("read error " + path.string());
    }

    return {data, fileLength};
};

} // namespace IO
