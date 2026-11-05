#pragma once

#include "asm_export.h"
#include "interpreter/code.h"

#include <stdint.h>

namespace Interpretation {

struct Frame {
    uintptr_t start;
};

inline ExecBytecodeInfo* GetPinnedBytecode(const void* framePointer)
{
    auto slot = reinterpret_cast<ExecBytecodeInfo* const*>(
        static_cast<const uint8_t*>(framePointer) - PINNED_BYTECODE_SLOT_OFFSET
    );
    return *slot;
}

} // namespace Interpretation
