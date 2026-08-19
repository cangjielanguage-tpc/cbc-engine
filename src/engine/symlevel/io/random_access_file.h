#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace IO {

/**
 * @class RandomAccessFile
 */
class RandomAccessFile {
public:
    virtual ~RandomAccessFile() = default;

    /**
     * @brief File length.
     */
    virtual size_t FileLength() const = 0;

    /**
     * @brief Read @p length bytes into @p array starting from @p position in file.
     *
     * @return the number of bytes read.
     */
    virtual size_t Peek(char* array, size_t position, size_t length) const = 0;

    /**
     * @brief Read @p length bytes into @p array starting from @p position in file.
     *
     * @throws runtime exception if end of stream is reached.
     */
    void Read(char* array, size_t position, size_t length) const;

    uint8_t ReadU8(size_t position);
    uint16_t ReadU16(size_t position);
    uint32_t ReadU32(size_t position);
    int32_t ReadS32(size_t position);
    uint64_t ReadU64(size_t position);
};

} // namespace IO
