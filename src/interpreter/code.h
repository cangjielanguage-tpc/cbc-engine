#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include "literals.h"
#include <cstdint>

namespace Interpretation {

struct Code {
    std::size_t const bytecodeSize;
    uint8_t* const bytecode;
    Interpretation::LiteralTable* const literals;
    // TODO: add offsets converter
};

struct ReferenceSlots {}; // TODO: implement reference maps

struct ExecBytecodeInfo {
    Code const code;
    uint16_t const savedIRegs;
    uint16_t const savedFRegs;
    uint16_t const untypedSlotCount;
    uint32_t const frameSize;
    ReferenceSlots references;
};

} // namespace Interpretation

#endif // INTERPRETER_CODE_H
