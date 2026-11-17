#pragma once

#include <cstdint>
#include <cstring>

namespace Utils {

using Float16 = _Float16;
static_assert(sizeof(Float16) == sizeof(uint16_t));

inline Float16 F64toF16(double value)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return static_cast<Float16>(value);
#else
    // FIXME: https://git.excelsior.internal/cbc/kanban/issues/265
    // support direct cast instead of double casting
    return static_cast<Float16>(static_cast<float>(value));
#endif
}

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
