#ifndef MATH_H
#define MATH_H

#include <cstdint>

namespace MathUtils {
    static bool IsNBits(uint64_t value, uint32_t bits) {
        if (bits == 64) {
            return true;
        } else {
            return ((value >> (bits - 1)) == 0);
        }
    }

    static bool IsNBitsSigned(int32_t value, uint32_t bits) {
        if (bits == 32) {
            return true;
        } else {
            // C++ have implementation-defined right shift for signed numbers until C++20.
            // We expect arithmetic shift.
            static_assert((-1 >> 16) == -1);
            auto extension = value >> (bits - 1);
            return (extension == 0) || (extension == -1);
        }
    }

    static bool IsNBitsSigned(uint32_t value, uint32_t bits) {
        return IsNBitsSigned(static_cast<int32_t>(value), bits);
    }

    static bool IsNBitsSigned(int64_t value, uint32_t bits) {
        if (bits == 64) {
            return true;
        } else {
            // C++ have implementation-defined right shift for signed numbers until C++20.
            // We expect arithmetic shift.
            static_assert((-1 >> 16) == -1);
            auto extension = value >> (bits - 1);
            return (extension == 0) || (extension == -1L);
        }
    }

    static bool IsNBitsSigned(uint64_t value, uint32_t bits) {
        return IsNBitsSigned(static_cast<int64_t>(value), bits);
    }

    static inline uint64_t SignExtend(uint64_t value, uint32_t bits) {
        uint64_t const m = 1UL << (bits - 1); // sign bit mask
        value = value & ((1UL << bits) - 1); // zero high bits
        return (value ^ m) - m;
    }
}

#endif // MATH_H
