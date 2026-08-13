#pragma once

#include <cstdint>

namespace Symlevel {

/// References and terms in CBC are referenced by index in the corresponding table.
template <typename T> struct RefId {
    static constexpr auto BIT_SIZE = 32;

    constexpr explicit RefId(uint32_t index) : value(index) {}

    operator uint32_t() const { return value; }

    uint32_t GetValue() const { return value; }

    bool operator==(const RefId& another) const { return value == another.value; }

private:
    uint32_t value;
};

} // namespace Symlevel
