#pragma once

#include "utils/assertion.h"
#include <cstdint>

namespace Symlevel {

template <typename T> class Index {
public:
    static constexpr uint64_t INDEX_SHIFT  = 0;
    static constexpr uint64_t REGION_SHIFT = 24;

    static constexpr uint64_t INDEX_MASK  = 0xffff'ff;
    static constexpr uint64_t REGION_MASK = 0xff;

    constexpr Index(uint32_t region, uint32_t index) : raw(0)
    {
        ASSERT((region & REGION_MASK) == region);
        ASSERT((index & INDEX_MASK) == index);

        raw = (region << REGION_SHIFT) | (index << INDEX_SHIFT);
    }

    explicit constexpr Index(uint32_t raw) : raw(raw) {}

    constexpr Index(Index const& another) : raw(another.raw) {}

    uint32_t Raw() const { return raw; }

    uint32_t GetRegion() const { return (raw >> REGION_SHIFT) & REGION_MASK; }

    uint32_t GetIndex() const { return (raw >> INDEX_SHIFT) & INDEX_MASK; }

    bool operator==(const Index& another) const { return raw == another.raw; }

private:
    uint32_t raw;
};

} // namespace Symlevel
