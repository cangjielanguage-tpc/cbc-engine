#include "runtime_frame.h"

#include "cbc/frame.h"
#include "interpreter/code.h"
#include "utils/math.h"

#include <limits>

namespace Cbc {

std::optional<ExpandedFrameLayout> ExpandFrame(
    const Interpretation::ExecBytecodeInfo& caller, uint32_t requiredInlineAreaSize
)
{
    if (caller.staticFrameSize > caller.frameSize) {
        return std::nullopt;
    }

    const auto alignment = Cbc::FRAME_ALIGNMENT;
    const auto maxSize   = std::numeric_limits<uint32_t>::max();
    if (requiredInlineAreaSize > maxSize - (alignment - 1U)) {
        return std::nullopt;
    }

    const auto inlineAreaSize = MathUtils::AlignUp(requiredInlineAreaSize, alignment);
    if (caller.staticFrameSize > maxSize - inlineAreaSize || caller.frameSize > maxSize - inlineAreaSize) {
        return std::nullopt;
    }

    return ExpandedFrameLayout {
        .frameSize       = caller.frameSize + inlineAreaSize,
        .staticFrameSize = caller.staticFrameSize + inlineAreaSize,
        .inlineAreaSize  = inlineAreaSize,
    };
}

bool HasSupportedInlineFrame(const Interpretation::ExecBytecodeInfo& callee)
{
    const auto count = [](Interpretation::NonVolatileRegs regs) {
        uint32_t count = 0;
        while (!regs.IsEmpty()) {
            regs.ExtractReg();
            ++count;
        }
        return count;
    };
    const auto saveSize = (count(callee.savedIRegs) + count(callee.savedFRegs)) * STACK_SLOT_SIZE;
    const auto untypedSize = callee.untypedSlotCount * STACK_SLOT_SIZE;
    const auto staticSize = callee.staticFrameSize;
    return staticSize >= saveSize + untypedSize && staticSize % STACK_SLOT_SIZE == 0 &&
           staticSize <= std::numeric_limits<uint32_t>::max() - (FRAME_ALIGNMENT - 1U) &&
           callee.frameSize == MathUtils::AlignUp(staticSize, FRAME_ALIGNMENT);
}

} // namespace Cbc
