#include "runtime_frame.h"

#include "cbc/frame.h"
#include "inlining/inline_plan.h"
#include "interpreter/code.h"
#include "utils/math.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <unordered_map>

namespace Cbc {

namespace {

InlineFramePlacement PlaceInlineFrame(
    const ExpandedFrameLayout& layout,
    const Interpretation::ExecBytecodeInfo& callee,
    uint32_t areaOffset,
    uint16_t callerIMask,
    uint16_t callerFMask,
    bool dedicated
)
{
    const auto frameShift = layout.staticFrameSize - layout.inlineAreaSize + areaOffset;
    InlineFramePlacement placement {
        .frameShift      = frameShift,
        .gcSlotByteShift = layout.inlineAreaSize - areaOffset - callee.staticFrameSize,
        .iRegSaveOffsets = {},
        .fRegSaveOffsets = {},
        .localsOffset    = static_cast<int32_t>(-static_cast<int64_t>(frameShift) - callee.staticFrameSize),
        .localsSize      = 0,
        .clearOnReturn   = dedicated,
    };
    auto offset = -static_cast<int64_t>(frameShift);
    // Reserve the callee's full save area in trampoline order.
    for (auto regs = callee.savedIRegs; !regs.IsEmpty();) {
        const auto reg  = regs.ExtractReg();
        offset         -= STACK_SLOT_SIZE;
        if ((callerIMask & (1U << reg)) != 0) {
            placement.iRegSaveOffsets[reg] = static_cast<int32_t>(offset);
        }
    }
    for (auto regs = callee.savedFRegs; !regs.IsEmpty();) {
        const auto reg  = regs.ExtractReg();
        offset         -= STACK_SLOT_SIZE;
        if ((callerFMask & (1U << reg)) != 0) {
            placement.fRegSaveOffsets[reg] = static_cast<int32_t>(offset);
        }
    }
    placement.localsSize = static_cast<uint32_t>(offset - placement.localsOffset);
    return placement;
}

} // namespace

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

    const auto inlineAreaSize  = MathUtils::AlignUp(requiredInlineAreaSize, alignment);
    const auto callerSlotShift = MathUtils::AlignUp(addedSaveSize, alignment);
    if (inlineAreaSize > maxSize - caller.frameSize || callerSlotShift > maxSize - caller.frameSize - inlineAreaSize) {
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
    const auto saveSize =
        (RegMask(callee.savedIRegs.Mask()).count() + RegMask(callee.savedFRegs.Mask()).count()) * STACK_SLOT_SIZE;
    const auto untypedSize = callee.untypedSlotCount * STACK_SLOT_SIZE;
    const auto staticSize  = callee.staticFrameSize;
    return staticSize >= saveSize + untypedSize && staticSize % STACK_SLOT_SIZE == 0 &&
           staticSize <= std::numeric_limits<uint32_t>::max() - (FRAME_ALIGNMENT - 1U) &&
           callee.frameSize == MathUtils::AlignUp(staticSize, FRAME_ALIGNMENT);
}

std::optional<InlineFramePlan> PlanInlineFrames(
    const Interpretation::ExecBytecodeInfo& caller, const std::vector<InlineCallSite>& sites
)
{
    const auto callerIMask     = caller.savedIRegs.Mask();
    const auto callerFMask     = caller.savedFRegs.Mask();
    auto mergedIMask           = callerIMask;
    auto mergedFMask           = callerFMask;
    uint32_t sharedAreaSize    = 0;
    uint32_t dedicatedAreaSize = 0;
    std::unordered_map<const Interpretation::ExecBytecodeInfo*, uint32_t> dedicatedFrameOffsets;
    for (const auto& site : sites) {
        const auto* callee = site.expectedCalleeBytecode;
        if (callee == nullptr || !HasSupportedInlineFrame(*callee)) {
            return std::nullopt;
        }
        if (!callee->gcInfo.positionalInfo.Empty() && !callee->gcInfo.refOffsets.Empty()) {
            // Frame-wide roots cannot share storage with a different layout.
            if (dedicatedFrameOffsets.try_emplace(callee, dedicatedAreaSize).second) {
                if (callee->staticFrameSize > std::numeric_limits<uint32_t>::max() - dedicatedAreaSize) {
                    return std::nullopt;
                }
                dedicatedAreaSize += callee->staticFrameSize;
            }
        } else {
            sharedAreaSize = std::max(sharedAreaSize, callee->staticFrameSize);
        }
        mergedIMask |= callee->savedIRegs.Mask();
        mergedFMask |= callee->savedFRegs.Mask();
    }

    if (sharedAreaSize > std::numeric_limits<uint32_t>::max() - dedicatedAreaSize) {
        return std::nullopt;
    }
    using RegMask = std::bitset<Interpretation::NonVolatileRegs::REGS_MAX_NUM>;
    const auto addedSaveSize =
        (RegMask(mergedIMask ^ callerIMask).count() + RegMask(mergedFMask ^ callerFMask).count()) * STACK_SLOT_SIZE;
    const auto requiredAreaSize = dedicatedAreaSize + sharedAreaSize;
    const auto layout           = ExpandFrame(caller, requiredAreaSize, addedSaveSize);
    if (!layout.has_value() ||
        -static_cast<int64_t>(caller.staticFrameSize) - layout->callerSlotShift - requiredAreaSize <
            std::numeric_limits<int32_t>::min()) {
        return std::nullopt;
    }

    InlineFramePlan plan { .layout = *layout, .savedIMask = mergedIMask, .savedFMask = mergedFMask };
    plan.placements.reserve(sites.size());
    for (const auto& site : sites) {
        const auto dedicated         = dedicatedFrameOffsets.find(site.expectedCalleeBytecode);
        const bool hasDedicatedFrame = dedicated != dedicatedFrameOffsets.end();
        const auto areaOffset        = hasDedicatedFrame ? dedicated->second : dedicatedAreaSize;
        plan.placements.push_back(PlaceInlineFrame(
            *layout, *site.expectedCalleeBytecode, areaOffset, callerIMask, callerFMask, hasDedicatedFrame
        ));
    }
    for (const auto& [callee, areaOffset] : dedicatedFrameOffsets) {
        const auto shift = layout->inlineAreaSize - areaOffset - callee->staticFrameSize;
        for (auto offset : callee->gcInfo.refOffsets) {
            plan.inlineRefOffsets.PushBack(shift + offset);
        }
    }
    std::sort(plan.inlineRefOffsets.begin(), plan.inlineRefOffsets.end());
    return plan;
}

} // namespace Cbc
