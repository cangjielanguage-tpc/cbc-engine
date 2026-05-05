#pragma once

#include <cstddef>
#include <cstdint>

#include "utils/assertion.h"

namespace IO {

struct FileId {
    static constexpr auto BIT_SIZE = 28;
    static constexpr auto MAX_ID = (1 << 28) - 1;

    const uint32_t id;

    FileId(uint32_t id) : id(id) { ASSERT(id <= MAX_ID); }

    inline operator std::uint32_t() const { return id; }

    inline operator std::size_t() const { return id; }

    inline bool operator==(const FileId& another) const {
        return id == another.id;
    }
};

} // namespace IO
