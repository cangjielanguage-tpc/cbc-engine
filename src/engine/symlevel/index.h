#pragma once

#include <cstdint>

namespace Symlevel {

/// References and terms in CBC are referenced by index, not offset.
/// These indicies are usually encoded by 16 bit in bytecode,
/// which is not enough for some cases.
///
/// To allow more references, CBC uses regions that
/// could implicitly extend 16-bit indicies up to 24 bit.

template <typename T> struct Index {
    static constexpr auto BIT_SIZE = 24;

    uint8_t const region;
    uint16_t const index;

    constexpr Index(uint8_t region, uint16_t index) : region(region), index(index) {}

    uint8_t GetRegion() const { return region; }

    uint32_t GetIndex() const { return index; }

    bool operator==(const Index& another) const {
        return region == another.region && index == another.index;
    }
};

} // namespace Symlevel
