#pragma once

#include "engine/symlevel/index.h"
#include "engine/symlevel/offset.h"
#include "stream_file_reader.h"
#include <cstdint>
#include <functional>

namespace IO {

template <typename T> class OffsetPool {
public:
    OffsetPool(uint32_t offset, uint32_t size) : offset(offset), size(size) {}

    Symlevel::Offset<T> QueryOffset(RandomAccessFile& file, uint32_t index) const
    {
        uint32_t idx = index;
        ASSERT(idx < size);

        uint32_t offs = offset + idx * sizeof(uint32_t);
        return Symlevel::Offset<T>(IO::StreamFileReader(file, offs).ReadU32());
    }

    void ForEach(RandomAccessFile& file, std::function<void(Symlevel::RefId<T>)> f) const
    {
        for (uint32_t i = offset; i < size; i += sizeof(uint32_t)) {
            f(Symlevel::RefId<T>(0, i / sizeof(uint32_t))); // FIXME?
        }
    }

private:
    uint32_t offset;
    uint32_t size;
};

} // namespace IO
