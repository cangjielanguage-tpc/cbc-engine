#pragma once

#include <cstdint>
#include <optional>

namespace Interpretation {
struct ExecBytecodeInfo;
}

namespace Cbc {

struct ExpandedFrameLayout {
    uint32_t frameSize;
    uint32_t staticFrameSize;
    uint32_t inlineAreaSize;
    uint32_t callerSlotShift;
};

// Reserve space above the caller's slots for new saves and below them for inline bodies.
std::optional<ExpandedFrameLayout> ExpandFrame(
    const Interpretation::ExecBytecodeInfo& caller, uint32_t requiredInlineAreaSize, uint32_t addedSaveSize = 0
);

bool HasSupportedInlineFrame(const Interpretation::ExecBytecodeInfo& callee);

} // namespace Cbc
