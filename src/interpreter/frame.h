#pragma once

#include <stdint.h>

namespace Interpretation {

struct Frame {
public:
    Frame(uintptr_t start) : start(start) {}

    uintptr_t start;
};

} // namespace Interpretation
