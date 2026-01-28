#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include <cstdint>
#include "literals.h"

namespace Interpretation {

struct Code {
    std::size_t bytecodeSize;
    uint8_t* bytecode;
    Interpretation::LiteralTable *literals;
};

} // Interpretation

#endif // INTERPRETER_CODE_H
