#pragma once

#include "random_access_file.h"
#include <fstream>
#include <stdexcept>


namespace IO {

/**
 * @class ByteArrayRandomAccessFile
 * @brief Simple realization of RandomAccessFile in which whole file is read to byte array.
 */
class ByteArrayRandomAccessFile : public RandomAccessFile {
public:

    ByteArrayRandomAccessFile(std::filesystem::path path): ByteArrayRandomAccessFile(path, OpenFile(path)) {}

    virtual ~ByteArrayRandomAccessFile()
    {
        RandomAccessFile::~RandomAccessFile();
        delete data;
    }

    virtual std::filesystem::path Path() const override
    {
        return path;
    }

    virtual size_t FileLength() const override
    {
        return fileLength;
    }

    virtual size_t Peek(char* array, size_t position, size_t length) const override;

private:

    ByteArrayRandomAccessFile(std::filesystem::path path, std::tuple<char*, size_t> data):
        path(std::move(path)),
        data(std::get<0>(data)),
        fileLength(std::get<1>(data)) {}

    const std::filesystem::path path;
    const char* data;
    const size_t fileLength;

    static std::tuple<char*, size_t> OpenFile(std::filesystem::path path);
};

} // namespace IO
