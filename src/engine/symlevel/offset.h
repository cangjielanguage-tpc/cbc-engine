#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Symlevel {

template <typename T> struct Offset {
    static constexpr auto BIT_SIZE   = 28;
    static constexpr auto MAX_OFFSET = (1 << BIT_SIZE) - 1;

    Offset(uint32_t value) : value(value) { ASSERTION(value <= MAX_OFFSET, "Offset is too big"); }

    operator uint32_t() const { return value; }

    uint32_t value;
};

} // namespace Symlevel
