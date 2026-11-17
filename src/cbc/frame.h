#pragma once

#include "utils/vector.h"
#include <cstdint>
#include <stdint.h>

namespace Cbc {

static uint32_t FRAME_ALIGNMENT = 16;
static uint32_t STACK_SLOT_SIZE = 8;

// `Execution::LoadTypeInfo` reads one incoming TypeInfo per term subterm (from
// IR1+i) and supports at most this many subterms. The rewriter rejects open
// terms beyond this limit instead of emitting a load that would abort.
static constexpr uint32_t MAX_LOAD_GENERIC_SUBTERMS = 6;

struct FrameLayout {
    Utils::Vector<uint32_t> typedOffset;
    Utils::Vector<uint32_t> refOffsets;
    uint32_t frameSize;

    int32_t UntypedSlotOffset(uint16_t us) const
    {
        return static_cast<int32_t>(static_cast<int64_t>(us) * STACK_SLOT_SIZE - static_cast<int64_t>(frameSize));
    }
};

} // namespace Cbc
