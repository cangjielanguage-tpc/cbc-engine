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

    ByteArrayRandomAccessFile(const char* data, size_t fileLength) : data(data), fileLength(fileLength) {}

    virtual ~ByteArrayRandomAccessFile()
    {
        RandomAccessFile::~RandomAccessFile();
        delete data;
    }

    virtual size_t FileLength() const override
    {
        return fileLength;
    }

    virtual size_t Peek(char* array, size_t position, size_t length) const override;

private:

    const char* data;
    const size_t fileLength;
};

} // namespace IO
