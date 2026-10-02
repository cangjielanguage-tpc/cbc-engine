#pragma once

#include <cstdint>
#include <stdint.h>
#include <unordered_map>
#include "utils/vector.h"

namespace Cbc {

static uint32_t FRAME_ALIGNMENT = 16;
static uint32_t STACK_SLOT_SIZE = 8;

struct FrameLayout {
    std::unordered_map<uint32_t, int32_t> typedOffset;
    Utils::Vector<int32_t> stackAllocSize;
    Utils::Vector<int32_t> refOffsets;
    uint32_t frameSize;
};

} // namespace Cbc
