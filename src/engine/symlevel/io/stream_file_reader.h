#pragma once

#include "random_access_file.h"
#include "utils/assertion.h"
#include <cassert>
#include <cstdint>


namespace IO {

/**
 * @class StreamFileReader
 * @brief Stream file reader from RandomAccessFile.
 *
 * This class provides necessary utilities to read various formats.
 */
class StreamFileReader {
public:

    StreamFileReader(RandomAccessFile& file, uint32_t position): file(file), position(position) {}
    StreamFileReader(RandomAccessFile* file, uint32_t position): file(*file), position(position) {}

    /**
     * @brief Returns current stream position.
     */
    uint32_t Position() const
    {
        return position;
    }

    /**
     * @brief Advance current stream position by @p values bytes.
     */
    void Advance(size_t value)
    {
        ASSERT(value >= 0);
        position += value;
    }

    void Read(char* array, size_t length)
    {
        file.Read(array, position, length);
        position += length;
    }

    void Read(char* array, uint32_t length)
    {
        Read(array, static_cast<size_t>(length));
    }

    size_t   ReadPtr() { return ReadValue<size_t>();   }

    uint8_t  ReadU8()  { return ReadValue<uint8_t>();  }
    uint16_t ReadU16() { return ReadValue<uint16_t>(); }
    uint32_t ReadU32() { return ReadValue<uint32_t>(); }
    uint64_t ReadU64() { return ReadValue<uint64_t>(); }

    uint32_t ReadULEB();
    int32_t  ReadSLEB();

    uint64_t ReadLongULEB();
    int64_t  ReadLongSLEB();


private:
    RandomAccessFile& file;
    uint32_t position;

    template <typename T> T ReadValue()
    {
        T value;
        Read(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }
};


} // namespace IO
