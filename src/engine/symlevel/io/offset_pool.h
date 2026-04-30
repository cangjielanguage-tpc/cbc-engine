#pragma once

#include "engine/symlevel/index.h"
#include "engine/symlevel/offset.h"
#include "stream_file_reader.h"
#include <cstdint>

namespace IO {

template <typename T>
class OffsetPool {
public:
    OffsetPool(uint32_t offset, uint32_t size) : offset(offset), size(size) {}

    Symlevel::Offset<T> QueryOffset(RandomAccessFile& file, uint32_t index) const
    {
        uint32_t idx = index;
        ASSERT(idx < size);

        uint32_t offs = offset + idx * sizeof(uint32_t);
        return Symlevel::Offset<T>(IO::StreamFileReader(file, offs).ReadU32());
    }

private:
    uint32_t offset;
    uint32_t size;
};

} // namespace IO
