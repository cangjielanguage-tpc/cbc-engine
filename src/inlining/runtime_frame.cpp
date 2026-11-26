#include "runtime_frame.h"

#include "cbc/frame.h"
#include "interpreter/code.h"
#include "utils/math.h"

#include <bitset>
#include <limits>

namespace Cbc {

std::optional<ExpandedFrameLayout> ExpandFrame(
    const Interpretation::ExecBytecodeInfo& caller, uint32_t requiredInlineAreaSize, uint32_t addedSaveSize
)
{
    if (caller.staticFrameSize > caller.frameSize) {
        return std::nullopt;
    }

    const auto alignment = Cbc::FRAME_ALIGNMENT;
    const auto maxSize   = std::numeric_limits<uint32_t>::max();
    if (requiredInlineAreaSize > maxSize - (alignment - 1U) || addedSaveSize > maxSize - (alignment - 1U)) {
        return std::nullopt;
    }

    const auto inlineAreaSize = MathUtils::AlignUp(requiredInlineAreaSize, alignment);
    const auto callerSlotShift = MathUtils::AlignUp(addedSaveSize, alignment);
    if (inlineAreaSize > maxSize - caller.frameSize ||
        callerSlotShift > maxSize - caller.frameSize - inlineAreaSize) {
        return std::nullopt;
    }

    const auto growth = inlineAreaSize + callerSlotShift;
    return ExpandedFrameLayout {
        .frameSize       = caller.frameSize + growth,
        .staticFrameSize = caller.staticFrameSize + growth,
        .inlineAreaSize  = inlineAreaSize,
        .callerSlotShift = callerSlotShift,
    };
}

bool HasSupportedInlineFrame(const Interpretation::ExecBytecodeInfo& callee)
{
    using RegMask = std::bitset<Interpretation::NonVolatileRegs::REGS_MAX_NUM>;
    const auto saveSize = (RegMask(callee.savedIRegs.Mask()).count() + RegMask(callee.savedFRegs.Mask()).count()) *
                          STACK_SLOT_SIZE;
    const auto untypedSize = callee.untypedSlotCount * STACK_SLOT_SIZE;
    const auto staticSize = callee.staticFrameSize;
    return staticSize >= saveSize + untypedSize && staticSize % STACK_SLOT_SIZE == 0 &&
           staticSize <= std::numeric_limits<uint32_t>::max() - (FRAME_ALIGNMENT - 1U) &&
           callee.frameSize == MathUtils::AlignUp(staticSize, FRAME_ALIGNMENT);
}

} // namespace Cbc
