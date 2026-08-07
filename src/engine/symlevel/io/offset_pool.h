#pragma once

#include "engine/symlevel/index.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/symlevel/offset.h"
#include "stream_file_reader.h"
#include "utils/iterators.h"
#include <cstdint>
#include <optional>

namespace IO {

template <typename T, uint32_t adjustment = 0> class OffsetPool {
    struct OffsetGenerator {
        OffsetPool<T, adjustment> const& op;
        uint32_t cursor;

        std::optional<Symlevel::RefId<T>> operator()()
        {
            if (cursor < op.size) {
                return Symlevel::RefId<T>(adjustment + cursor++);
            } else {
                return std::nullopt;
            }
        }
    };

public:
    OffsetPool(uint32_t offset, uint32_t size) : offset(offset), size(size) {}

    Symlevel::Offset<T> QueryOffset(RandomAccessFile& file, uint32_t index) const
    {
        uint32_t idx = index - adjustment;
        ASSERT(idx < size);

        uint32_t offs = offset + idx * sizeof(uint32_t);
        return Symlevel::Offset<T>(IO::StreamFileReader(file, offs).ReadU32());
    }

    Iterators::SimpleRange<OffsetGenerator> RefIds() const
    {
        return Iterators::MakeRange(
            OffsetGenerator {
                .op     = *this,
                .cursor = 0,
            }
        );
    }

private:
    uint32_t offset;
    uint32_t size;
};

} // namespace IO
