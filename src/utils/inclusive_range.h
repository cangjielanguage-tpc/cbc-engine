#pragma once

#include "assertion.h"
#include "math.h"
#include <cstdint>

struct InclusiveRange {
    const uint16_t start;
    const uint16_t end;

    constexpr InclusiveRange(uint16_t start, uint16_t end) : start(start), end(end) { ASSERT(start <= end); }

    constexpr inline bool Covers(uint16_t v) const { return start <= v && v <= end; }

    constexpr inline int Length() const { return end - start + 1; }
};
