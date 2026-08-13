#pragma once

#include <cstdint>

namespace Symlevel {

/// References and terms in CBC are referenced by index in the corresponding table.
template <typename T> struct RefId {
    static constexpr uint64_t BIT_SIZE = 32;
    static constexpr uint64_t MASK     = (1llu << BIT_SIZE) - 1;

    constexpr explicit RefId(uint32_t index) : value(index) {}

    operator uint32_t() const { return value; }

    uint32_t GetValue() const { return value; }

    bool operator==(const RefId& another) const { return value == another.value; }

private:
    uint32_t value;
};

} // namespace Symlevel
