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

    Offset<T> operator+(Offset<T>& that) const { return Offset<T>(value + that.value); }

    Offset<T> operator+(uint32_t that) const { return Offset<T>(value + that); }

    operator uint32_t() const { return value; }

    const uint32_t value;
};

} // namespace Symlevel
