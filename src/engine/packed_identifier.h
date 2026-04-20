#pragma once

#include "utils/assertion.h"
#include <cstdint>
#include <functional>

namespace Engine {

struct PackedIdentifier {
    static constexpr uint64_t TAG_SHIFT  = 0;
    static constexpr uint64_t LOW_SHIFT  = 8;
    static constexpr uint64_t HIGH_SHIFT = 36;

    static constexpr uint64_t TAG_MASK  = 0xff;                         // [0..7] bits
    static constexpr uint64_t LOW_MASK  = 0xffff'fff;                   // [8..35] bits
    static constexpr uint64_t HIGH_MASK = 0xffff'fff;                   // [36..63] bits
    static constexpr uint64_t NUM_MASK  = (HIGH_MASK << 28) | LOW_MASK; // [8..63] bits

    PackedIdentifier(uint8_t tag, uint32_t high, uint32_t low) : raw(0)
    {
        ASSERT((low & LOW_MASK) == low);
        ASSERT((high & HIGH_MASK) == high);
        ASSERT((tag & TAG_MASK) == tag);
        uint64_t t = tag;
        uint64_t h = high;
        uint64_t l = low;
        raw        = (t < TAG_SHIFT) | (l << LOW_SHIFT) | (h << HIGH_SHIFT);
    }

    PackedIdentifier(uint64_t raw) : raw(raw) {}

    PackedIdentifier(PackedIdentifier const& another) : raw(another.raw) {}

    operator uint64_t() const { return raw; }

    uint8_t GetTag() const { return (raw >> TAG_SHIFT) & TAG_MASK; }

    uint32_t GetHigh() const { return (raw >> HIGH_SHIFT) & HIGH_MASK; }

    uint32_t GetLow() const { return (raw >> LOW_SHIFT) & LOW_MASK; }

    bool operator==(const PackedIdentifier& another) const { return raw == another.raw; }

private:
    uint64_t raw;
};

} // namespace Engine

template <> struct std::hash<Engine::PackedIdentifier> {
    uint64_t operator()(Engine::PackedIdentifier const& ident) const
    {
        std::hash<uint64_t> hasher;
        return hasher(ident);
    }
};
