#include "offsets_index.h"

#include "emitter/emitter.h"

#include <algorithm>

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

bool InstructionOffsetsIndex::OffsetsAreInAscendingOrder(Utils::Vector<Offset> const& offsets)
{
    return std::is_sorted(offsets.begin(), offsets.end());
}

}; // namespace Cbc
