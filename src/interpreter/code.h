#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include <cstdint>
#include "literals.h"

namespace Interpretation {

struct Code {
    std::size_t const bytecodeSize;
    uint8_t* const bytecode;
    Interpretation::LiteralTable* const literals;
    // TODO: add offsets converter
};

struct ReferenceSlots {}; // TODO: implement reference maps

struct FuHDescriptor {
    Code const code;
    uint16_t const usedNonVolIRegs;
    uint16_t const usedNonVolIFegs;
    uint16_t const untypedSlotCount;
    uint16_t const typedPartSizeInSlots; // TODO: make it bigger?
};

} // Interpretation

#endif // INTERPRETER_CODE_H
