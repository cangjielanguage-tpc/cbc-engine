#pragma once

#include "utils/vector.h"
#include <cstdint>
#include <stdint.h>

namespace Cbc {

static uint32_t FRAME_ALIGNMENT = 16;
static uint32_t STACK_SLOT_SIZE = 8;

struct FrameLayout {
    Utils::Vector<uint32_t> typedOffset;
    Utils::Vector<uint32_t> refOffsets;
    uint32_t frameSize;

    int32_t UntypedSlotOffset(uint16_t us) const { return us * STACK_SLOT_SIZE - frameSize; }
};

} // namespace Cbc
