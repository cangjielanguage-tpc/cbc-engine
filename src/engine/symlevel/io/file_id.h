#pragma once

#include <cstddef>
#include <cstdint>

#include "utils/assertion.h"

namespace IO {

struct FileId {
    static constexpr auto BIT_SIZE = 24;
    static constexpr auto MASK     = (1 << BIT_SIZE) - 1;

    uint32_t id;

    explicit FileId(uint32_t id) : id(id) { ASSERT((id & MASK) == id); }

    inline operator std::uint32_t() const { return id; }

    inline operator std::size_t() const { return id; }

    inline bool operator==(const FileId& another) const { return id == another.id; }
};

} // namespace IO
