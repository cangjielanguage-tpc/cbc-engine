#pragma once

#include "random_access_file.h"
#include "utils/vector.h"

#include <utility>

namespace IO {

/**
 * @class ByteArrayRandomAccessFile
 * @brief Simple implementation of RandomAccessFile that reads whole file to byte array.
 */
class ByteArrayRandomAccessFile : public RandomAccessFile {
public:
    explicit ByteArrayRandomAccessFile(Utils::Vector<char> data) : data(std::move(data)) {}

    ByteArrayRandomAccessFile(ByteArrayRandomAccessFile const&) = delete;
    ByteArrayRandomAccessFile& operator=(ByteArrayRandomAccessFile const&) = delete;

    virtual ~ByteArrayRandomAccessFile() = default;

    virtual size_t FileLength() const override { return data.Size(); }

    virtual size_t Peek(char* array, size_t position, size_t length) const override;

private:
    Utils::Vector<char> data;
};

} // namespace IO
