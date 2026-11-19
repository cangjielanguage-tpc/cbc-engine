#pragma once

#include "utils/assertion.h"
#include "utils/vector.h"

#include <optional>
#include <stdint.h>
#include <unistd.h>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Cbc {

namespace Emitter {
class Emitter;
class Label;
} // namespace Emitter

enum InstructionType {
    CBC,
    REWRITTEN
};

struct RewrittenCodeEdit {
    uint32_t oldBegin;
    uint32_t oldEnd;
    uint32_t newSize;
};

std::optional<uint32_t> RelocateRewrittenPosition(
    uint32_t oldPosition, const std::vector<RewrittenCodeEdit>& edits
);

class InstructionOffsetsIndex {
    using Offset = uint32_t;

public:
    static InstructionOffsetsIndex Create(
        Emitter::Emitter const& emitter, std::unordered_map<ssize_t, Emitter::Label> labels
    );

    int Length();

    bool IsEmpty() { return Length() == 0; }

    std::optional<Offset> FindMappedOffset(InstructionType type, Offset srcOffset) const;

    std::optional<Offset> FindContainingCbcOffset(Offset rtOffset) const;

    InstructionOffsetsIndex RelocateRewrittenOffsets(const std::vector<RewrittenCodeEdit>& edits) const;

private:
    Utils::Vector<Offset> cbcOffsets = {};
    Utils::Vector<Offset> rtOffsets  = {};

    bool OffsetsAreInAscendingOrder(Utils::Vector<Offset> const& offsets);
};

}; // namespace Cbc
