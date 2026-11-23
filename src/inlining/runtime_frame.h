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
};

// Reserve an area below the caller's existing static frame. The area is kept
// separate from the logical slot namespace until slot operands and metadata
// are relocated by the inliner.
std::optional<ExpandedFrameLayout> ExpandFrame(
    const Interpretation::ExecBytecodeInfo& caller, uint32_t requiredInlineAreaSize
);

bool HasSupportedInlineFrame(const Interpretation::ExecBytecodeInfo& callee);

} // namespace Cbc
