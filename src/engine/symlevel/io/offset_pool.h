#pragma once

#include "engine/identifiers.h"
#include "engine/symlevel/index.h"
#include "engine/symlevel/io/file_id.h"
#include <cstdint>

namespace IO {

struct ErasedOffsetPool {
    IO::FileId file;
    uint32_t offset;
    uint32_t size;
    uint32_t adjustment;
};

template <typename T, uint32_t adjustment = 0> struct OffsetPool {
    static constexpr uint32_t ADJUSTMENT = adjustment;

    struct RefIdIterator {
        IO::FileId file;
        uint32_t id;

        Engine::RefIdentifier<T> operator*() const
        {
            return Engine::RefIdentifier<T>(Symlevel::RefId<T>(id + adjustment), file);
        }

        RefIdIterator& operator++()
        {
            id++;
            return *this;
        }

        bool operator!=(RefIdIterator const& another) const { return id != another.id; }
    };

    OffsetPool(IO::FileId file, uint32_t offset, uint32_t size) : file(file), offset(offset), size(size) {}

    RefIdIterator begin() const { return RefIdIterator { file, 0 }; }

    RefIdIterator end() const { return RefIdIterator { file, size }; }

    ErasedOffsetPool Erased() const { return { file, offset, size, adjustment }; }

private:
    IO::FileId file;
    uint32_t offset;
    uint32_t size;
};

} // namespace IO
