#pragma once

#include "cbc/isa.h"
#include "utils/vector.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace Interpretation {
struct ExecBytecodeInfo;
}

namespace Cbc {

struct InlineCallSite;

struct ExpandedFrameLayout {
    uint32_t frameSize;
    uint32_t staticFrameSize;
    uint32_t inlineAreaSize;
    uint32_t callerSlotShift;
};

struct InlineFramePlacement {
    uint32_t frameShift;      // Distance from the caller's frame top to this inline frame.
    uint32_t gcSlotByteShift; // Distance from the new static frame bottom to the callee's bottom.
    // Offsets from the caller's frame top. Zero means this register needs no inline save.
    std::array<int32_t, IReg::COUNT> iRegSaveOffsets;
    std::array<int32_t, FReg::COUNT> fRegSaveOffsets;
    int32_t localsOffset;
    uint32_t localsSize;
    bool clearOnReturn;
};

struct InlineFramePlan {
    ExpandedFrameLayout layout;
    uint16_t savedIMask;
    uint16_t savedFMask;
    std::vector<InlineFramePlacement> placements; // In call-site order.
    Utils::Vector<uint32_t> inlineRefOffsets;
};

// Reserve space above the caller's slots for new saves and below them for inline bodies.
std::optional<ExpandedFrameLayout> ExpandFrame(
    const Interpretation::ExecBytecodeInfo& caller, uint32_t requiredInlineAreaSize, uint32_t addedSaveSize = 0
);

bool HasSupportedInlineFrame(const Interpretation::ExecBytecodeInfo& callee);

std::optional<InlineFramePlan> PlanInlineFrames(
    const Interpretation::ExecBytecodeInfo& caller, const std::vector<InlineCallSite>& sites
);

} // namespace Cbc
