#pragma once

#include "utils/assertion.h"

#include <optional>
#include <stdint.h>
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

class InstructionOffsetsIndex {
    using Offset = uint32_t;

public:
    static InstructionOffsetsIndex Create(
        Emitter::Emitter const& emitter, std::unordered_map<ssize_t, Emitter::Label> labels
    );

    int Length();

    bool IsEmpty() { return Length() == 0; }

    std::optional<Offset> FindMappedOffset(InstructionType type, Offset srcOffset) const;

private:
    std::vector<Offset> cbcOffsets = {};
    std::vector<Offset> rtOffsets  = {};

    bool OffsetsAreInAscendingOrder(std::vector<Offset> offsets);
};

}; // namespace Cbc
