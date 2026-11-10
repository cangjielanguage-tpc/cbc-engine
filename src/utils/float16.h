#pragma once

#include <cstdint>
#include <cstring>

namespace Utils {

using Float16 = _Float16;
static_assert(sizeof(Float16) == sizeof(uint16_t));

inline Float16 Float16FromBits(uint16_t bits)
{
    Float16 value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline uint16_t Float16Bits(Float16 value)
{
    uint16_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

} // namespace Utils
