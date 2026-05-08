#pragma once

#include <cstdint>

namespace Symlevel {

/// References and terms in CBC are referenced by index, not offset.
/// These indicies are usually encoded by 16 bit in bytecode,
/// which is not enough for some cases.
///
/// To allow more references, CBC uses regions that
/// could implicitly extend 16-bit indicies up to 24 bit.

template <typename T> struct RefId {
    static constexpr auto BIT_SIZE = 24;

    constexpr RefId(uint8_t region, uint16_t index) : region(region), index(index) {}

    uint8_t GetRegion() const { return region; }

    uint32_t GetIndex() const { return index; }

    bool operator==(const RefId& another) const { return region == another.region && index == another.index; }

private:
    uint8_t region;
    uint16_t index;
};

} // namespace Symlevel
