#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Symlevel {

template <typename T> struct Offset {
    static constexpr uint64_t BIT_SIZE = 32;
    static constexpr uint64_t MASK     = (1llu << BIT_SIZE) - 1;

    Offset(uint32_t value) : value(value) {}

    bool operator==(const Offset& another) const { return value == another.value; }

    operator uint32_t() const { return value; }

    uint32_t value;
};

} // namespace Symlevel
