#pragma once

#include <cstdint>
#include <cstring>
namespace Bits {

template <typename T>
inline uint64_t Raw64(T const& val) {
    static_assert(sizeof(T) == sizeof(uint64_t));
    uint64_t value;
    std::memcpy(&value, &val, sizeof(T));
    return value;
}

template <typename T>
inline uint32_t Raw32(T const& val) {
    static_assert(sizeof(T) == sizeof(uint32_t));
    uint32_t value;
    std::memcpy(&value, &val, sizeof(T));
    return value;
}

}
