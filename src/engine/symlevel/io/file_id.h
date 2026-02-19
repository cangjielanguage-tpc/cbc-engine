#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace IO {

struct FileId {
    static constexpr auto MAX_SIZE = (1 << 24);

    const uint32_t id;

    FileId(uint32_t id) : id(id) { ASSERT(id < MAX_SIZE); }

    operator std::uint32_t() const { return id; }

    operator std::size_t() const { return id; }
};

} // namespace IO
