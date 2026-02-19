#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Symlevel {

template <typename T> struct Offset {
    const uint32_t value;

    Offset(uint32_t value) : value(value) {}

    Offset(std::size_t value) : value(static_cast<uint32_t>(value))
    {
        ASSERTION(value < UINT32_MAX, "Offset is too big");
    }
};

} // namespace Symlevel
