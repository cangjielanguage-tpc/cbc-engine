#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Symlevel {

template <typename T> struct Offset {
    static constexpr auto MAX_OFFSET = (1 << 24) - 1;

    Offset(uint32_t value) : value(value) { ASSERTION(value <= MAX_OFFSET, "Offset is too big"); }

    Offset(std::size_t value) : value(static_cast<uint32_t>(value))
    {
        ASSERTION(value <= MAX_OFFSET, "Offset is too big");
    }

    operator uint32_t() const { return value; }

    const uint32_t value;
};

} // namespace Symlevel
