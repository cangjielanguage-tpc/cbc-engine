#pragma once

#include "emitter/emitter.h"
#include "utils/assertion.h"

#include <stdint.h>
#include <variant>
#include <vector>

namespace Cbc {

enum InstructionType {
    CBC,
    REWRITTEN
};

class InstructionOffsetsIndex {
    using Offset = uint32_t;

public:
    InstructionOffsetsIndex(): cbcOffsets({}), rtOffsets({}), state(BUILDING) {}

    int Length();

    bool IsEmpty() { return Length() == 0; }

    void Build(Emitter::Emitter& emitter, std::unordered_map<ssize_t, Emitter::Label> labels);

    Offset FindMappedOffset(InstructionType type, Offset srcOffset) {
        return FindMappedOffset(type, srcOffset, false);
    }

    Offset FindMappedOffset(InstructionType type, Offset srcOffset, bool failIfNotFound);

private:

    std::vector<Offset> cbcOffsets;
    std::vector<Offset> rtOffsets;

    constexpr static Offset UNKNOWN_OFFSET = -1;

    bool OffsetsAreInAscendingOrder(std::vector<Offset> offsets);

    enum State { BUILDING, READY } state;
};

}; // namespace Cbc
