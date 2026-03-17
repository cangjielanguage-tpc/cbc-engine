#pragma once

#include "engine/symlevel/index.h"
#include "engine/symlevel/offset.h"
#include "stream_file_reader.h"

namespace IO {

class OffsetPool {
public:
    OffsetPool(uint32_t offset, uint32_t size) : offset(offset), size(size), startIdx(0) {}

    OffsetPool(uint32_t offset, uint32_t size, uint32_t startIdx) : offset(offset), size(size), startIdx(startIdx) {}

    template <typename T> Symlevel::Offset<T> QueryOffset(RandomAccessFile& file, Symlevel::Index<T> index) const
    {
        uint32_t idx = index.index - startIdx;
        ASSERT(idx < size);

        uint32_t offs = offset + idx * sizeof(uint32_t);
        return Symlevel::Offset<T>(IO::StreamFileReader(file, offs).ReadU32());
    }

    uint32_t QueryOffset(RandomAccessFile& file, uint32_t index) const
    {
        uint32_t idx = index - startIdx;
        ASSERT(idx < size);

        uint32_t offs = offset + idx * sizeof(uint32_t);
        return IO::StreamFileReader(file, offs).ReadU32();
    }

private:
    uint32_t offset;
    uint32_t size;
    uint32_t startIdx;
};

} // namespace IO
