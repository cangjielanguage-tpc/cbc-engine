#pragma once

#include <stdint.h>

namespace Interpretation {

static uint32_t FRAME_ALIGNMENT = 16;
static uint32_t STACK_SLOT_SIZE = 8;

struct Frame {
public:
    Frame(uintptr_t start) : start(start) {}

    uintptr_t start;
};

} // namespace Interpretation
