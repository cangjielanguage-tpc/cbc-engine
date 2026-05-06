#include "offsets_index.h"

#include <algorithm>

namespace Cbc {

using Offset = uint32_t;

int InstructionOffsetsIndex::Length()
{
    ASSERTION(cbcOffsets.size() == rtOffsets.size(), "vectors length invariant violation");
    return cbcOffsets.size();
}

void InstructionOffsetsIndex::Build(Emitter::Emitter& emitter, std::unordered_map<ssize_t, Emitter::Label> labels)
{   
    cbcOffsets.reserve(labels.size());
    rtOffsets.reserve(labels.size());

    std::vector<std::pair<Offset, Offset>> tmp;
    tmp.reserve(labels.size());

    std::transform(labels.begin(), labels.end(), std::back_inserter(tmp),
        [&emitter](const auto& pair) {
            return std::make_pair(
                static_cast<Offset>(pair.first), 
                emitter.LabelPosition(pair.second));
        }
    );

    std::sort(tmp.begin(), tmp.end(), [](const auto& a, const auto& b) {
        return a.first < b.first; // sort by cbc offsets
    });

    for (const auto& [cbcOffset, rtOffset] : tmp) {
        cbcOffsets.push_back(cbcOffset);
        rtOffsets.push_back(rtOffset);
    }

    ASSERTION(cbcOffsets.size() == rtOffsets.size(),  "Wrong number of elements");
    ASSERTION(OffsetsAreInAscendingOrder(cbcOffsets), "CBC instruction offsets invariant violation");
    ASSERTION(OffsetsAreInAscendingOrder(rtOffsets),  "REWRITTEN instruction offsets invariant violation");

    state = READY;
}

Offset InstructionOffsetsIndex::FindMappedOffset(InstructionType type, Offset srcOffset, bool failIfNotFound)
{
    ASSERTION(state == READY, "Incorrect state");

    std::vector<Offset>& searchVec = type == CBC ? cbcOffsets : rtOffsets;
    std::vector<Offset>& resultVec = type == CBC ? rtOffsets : cbcOffsets;

    auto it = std::lower_bound(searchVec.begin(), searchVec.end(), srcOffset);
    if (it == searchVec.end() || *it != srcOffset) {
        if (failIfNotFound) {
            FATAL("Offset is not found: %u", srcOffset);
        }
        return UNKNOWN_OFFSET;
    }

    return resultVec[std::distance(searchVec.begin(), it)];
}

bool InstructionOffsetsIndex::OffsetsAreInAscendingOrder(std::vector<Offset> offsets)
{
    return std::is_sorted(offsets.begin(), offsets.end());
}


}; // namespace Cbc
