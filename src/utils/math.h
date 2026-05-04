#pragma once

#include <climits>
#include <cmath>
#include <cstdint>

namespace MathUtils {
static uint32_t AlignDown(uint32_t value, uint32_t alignment) { return (value / alignment) * alignment; }

static uint32_t AlignUp(uint32_t value, uint32_t alignment) { return AlignDown(value + alignment - 1, alignment); }

static bool IsNBits(uint64_t value, uint32_t bits)
{
    if (bits == 64) {
        return true;
    } else {
        return (value >> bits) == 0;
    }
}

static bool IsNBitsSigned(uint32_t value, uint32_t bits) { return IsNBitsSigned(static_cast<int32_t>(value), bits); }

template <typename T> static bool IsNBitsSigned(T value, uint32_t bits)
{
    if (bits == sizeof(T) * CHAR_BIT) {
        return true;
    } else {
        // C++ have implementation-defined right shift for signed numbers until C++20.
        // We expect arithmetic shift.
        static_assert((-1 >> 16) == -1);
        auto extension = value >> (bits - 1);
        return (extension == 0) || (extension == static_cast<T>(-1));
    }
}

static bool IsNBitsSigned(uint64_t value, uint32_t bits) { return IsNBitsSigned(static_cast<int64_t>(value), bits); }

template <typename T> static inline T SignExtend(T value, uint32_t bits)
{
    if (bits == sizeof(T) * CHAR_BIT)
        return value;
    T const one = static_cast<T>(1);
    T const m   = one << (bits - 1);           // sign bit mask
    value       = value & ((one << bits) - 1); // zero high bits
    return (value ^ m) - m;
}

static inline uint32_t RightNBits32(uint32_t n) { return static_cast<uint32_t>(n == 32 ? -1 : ((1L << n) - 1)); }

static inline uint64_t RightNBits64(uint32_t n) { return static_cast<uint64_t>(n == 64 ? -1L : ((1L << n) - 1)); }

static inline uint32_t ZeroExtend(uint32_t value, uint32_t bits) { return value & RightNBits32(bits); }

static inline uint64_t ZeroExtend(uint64_t value, uint32_t bits) { return value & RightNBits64(bits); }

static inline uint32_t RotateRight32(uint32_t value, uint32_t dist)
{
    return ((value >> dist) & RightNBits32(32 - dist)) | (value << (32 - dist));
}

static inline uint64_t RotateRight64(uint64_t value, uint32_t dist)
{
    return ((value >> dist) & RightNBits64(64 - dist)) | (value << (64 - dist));
}
} // namespace MathUtils
