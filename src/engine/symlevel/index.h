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
    static constexpr auto BIT_SIZE = 32;

    constexpr RefId(uint32_t index) : index(index) {}

    uint32_t GetIndex() const { return index; }

    bool operator==(const RefId& another) const { return index == another.index; }

private:
    uint32_t index;
};

} // namespace Symlevel
