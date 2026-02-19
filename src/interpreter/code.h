#ifndef INTERPRETER_CODE_H
#define INTERPRETER_CODE_H

#include "literals.h"
#include <cstdint>

namespace Interpretation {

struct Code {
    std::size_t bytecodeSize;
    uint8_t* bytecode;
    Interpretation::LiteralTable* literals;
};

} // namespace Interpretation

#endif // INTERPRETER_CODE_H
