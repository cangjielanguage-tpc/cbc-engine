#ifndef INTERPRETER_LITERALS_H
#define INTERPRETER_LITERALS_H

#include <cstdint>

namespace Interpretation {

union Literal {
    int32_t i32;
    int64_t i64;
    uintptr_t uintptr;
    uint32_t u32;
    uint64_t u64;
};

struct LiteralTable {
    std::size_t size;
    Literal table[]; // tail array
};

} // Interpretation

#endif // INTERPRETER_LITERALS_H
