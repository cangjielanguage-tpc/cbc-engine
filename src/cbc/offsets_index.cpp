#include "offsets_index.h"

#include "emitter/emitter.h"

#include <algorithm>
#include <limits>

namespace Cbc {

using Offset = uint32_t;

int InstructionOffsetsIndex::Length()
{
    ASSERTION(cbcOffsets.Size() == rtOffsets.Size(), "vectors length invariant violation");
    return cbcOffsets.Size();
}

InstructionOffsetsIndex InstructionOffsetsIndex::Create(
    Emitter::Emitter const& emitter, std::unordered_map<ssize_t, Emitter::Label> labels
)
{
    InstructionOffsetsIndex index;
    auto& cbcOffsets = index.cbcOffsets;
    auto& rtOffsets  = index.rtOffsets;

    cbcOffsets.Reserve(labels.size());
    rtOffsets.Reserve(labels.size());

    Utils::Vector<std::pair<Offset, Offset>> tmp;
    tmp.Reserve(labels.size());

    for (const auto& pair : labels) {
        tmp.EmplaceBack(static_cast<Offset>(pair.first), emitter.LabelPosition(pair.second));
    }

    std::sort(tmp.begin(), tmp.end(), [](const auto& a, const auto& b) {
        return a.first < b.first; // sort by cbc offsets
    });

    for (const auto& [cbcOffset, rtOffset] : tmp) {
        cbcOffsets.PushBack(cbcOffset);
        rtOffsets.PushBack(rtOffset);
    }

    ASSERTION(cbcOffsets.Size() == rtOffsets.Size(), "Wrong number of elements");
    ASSERTION(index.OffsetsAreInAscendingOrder(cbcOffsets), "CBC instruction offsets invariant violation");
    ASSERTION(index.OffsetsAreInAscendingOrder(rtOffsets), "REWRITTEN instruction offsets invariant violation");

    return index;
}

std::optional<Offset> InstructionOffsetsIndex::FindMappedOffset(InstructionType type, Offset srcOffset) const
{
    const Utils::Vector<Offset>& searchVec = type == CBC ? cbcOffsets : rtOffsets;
    const Utils::Vector<Offset>& resultVec = type == CBC ? rtOffsets : cbcOffsets;

    auto it = std::lower_bound(searchVec.begin(), searchVec.end(), srcOffset);
    if (it == searchVec.end() || *it != srcOffset) {
        return std::nullopt;
    }

    return resultVec[std::distance(searchVec.begin(), it)];
}

std::optional<Offset> InstructionOffsetsIndex::FindContainingCbcOffset(Offset rtOffset) const
{
    auto it = std::upper_bound(rtOffsets.begin(), rtOffsets.end(), rtOffset);
    if (it == rtOffsets.begin()) {
        return std::nullopt;
    }
    --it;
    return cbcOffsets[std::distance(rtOffsets.begin(), it)];
}

std::optional<uint32_t> RelocateRewrittenPosition(uint32_t oldPosition, const std::vector<RewrittenCodeEdit>& edits)
{
    int64_t delta        = 0;
    uint32_t previousEnd = 0;
    for (const auto& edit : edits) {
        ASSERT(edit.oldBegin <= edit.oldEnd);
        ASSERT(previousEnd <= edit.oldBegin);
        previousEnd = edit.oldEnd;

        if (oldPosition < edit.oldBegin) {
            break;
        }
        if (oldPosition == edit.oldBegin) {
            const auto relocated = static_cast<int64_t>(oldPosition) + delta;
            ASSERT(0 <= relocated && relocated <= std::numeric_limits<uint32_t>::max());
            return static_cast<uint32_t>(relocated);
        }
        if (oldPosition < edit.oldEnd) {
            return std::nullopt;
        }

        delta += static_cast<int64_t>(edit.newSize) - static_cast<int64_t>(edit.oldEnd - edit.oldBegin);
    }

    const auto relocated = static_cast<int64_t>(oldPosition) + delta;
    ASSERT(0 <= relocated && relocated <= std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(relocated);
}

InstructionOffsetsIndex InstructionOffsetsIndex::RelocateRewrittenOffsets(const std::vector<RewrittenCodeEdit>& edits
) const
{
    auto relocated = *this;
    for (auto& offset : relocated.rtOffsets) {
        auto newOffset = RelocateRewrittenPosition(offset, edits);
        ASSERTION(newOffset.has_value(), "Mapped runtime offset is inside a replaced instruction");
        offset = *newOffset;
    }
    ASSERTION(relocated.OffsetsAreInAscendingOrder(relocated.rtOffsets), "Relocated offsets are not sorted");
    return relocated;
}

bool InstructionOffsetsIndex::OffsetsAreInAscendingOrder(Utils::Vector<Offset> const& offsets)
{
    return std::is_sorted(offsets.begin(), offsets.end());
}

}; // namespace Cbc
