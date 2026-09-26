#pragma once

#include "utils/assertion.h"
#include "utils/vector.h"

#include <optional>
#include <stdint.h>
#include <unistd.h>
#include <unordered_map>
#include <variant>

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
    Utils::Vector<Offset> cbcOffsets = {};
    Utils::Vector<Offset> rtOffsets  = {};

    bool OffsetsAreInAscendingOrder(Utils::Vector<Offset> const& offsets);
};

}; // namespace Cbc
