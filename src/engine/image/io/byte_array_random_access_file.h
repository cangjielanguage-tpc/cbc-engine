#pragma once

#include "random_access_file.h"

namespace IO {

/**
 * @class ByteArrayRandomAccessFile
 * @brief Simple implementation of RandomAccessFile that reads whole file to byte array.
 */
class ByteArrayRandomAccessFile : public RandomAccessFile {
public:
    ByteArrayRandomAccessFile(const char* data, size_t fileLength) : data(data), fileLength(fileLength) {}

    virtual ~ByteArrayRandomAccessFile()
    {
        RandomAccessFile::~RandomAccessFile();
        delete data;
    }

    virtual size_t FileLength() const override { return fileLength; }

    virtual size_t Peek(char* array, size_t position, size_t length) const override;

private:
    const char* data;
    const size_t fileLength;
};

} // namespace IO
