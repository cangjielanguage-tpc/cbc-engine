#pragma once

#include <cstdint>
#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace Cbc {

static uint32_t FRAME_ALIGNMENT = 16;
static uint32_t STACK_SLOT_SIZE = 8;

struct FrameLayout {
    std::unordered_map<uint32_t, uint32_t> typedOffset;
    std::vector<uint32_t> stackAllocSize;
    std::vector<uint32_t> refOffsets;
    uint32_t untypedStackSize;
    uint32_t frameSize;
};

} // namespace Cbc
