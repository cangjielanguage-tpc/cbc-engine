#ifndef INTERPRETER_LITERALS_H
#define INTERPRETER_LITERALS_H

#include <cstddef>
#include <cstdint>

namespace Interpretation {

union Literal {
    int32_t i32;
    int64_t i64;
    uintptr_t uintptr;
    uint32_t u32;
    uint64_t u64;

    uint8_t raw[8];
};

constexpr auto LITERAL_SIZE = sizeof(Literal);

struct LiteralTable {
    std::size_t _byteSize;
    uint8_t _table[]; // tail array

    std::size_t size() const { return _byteSize / LITERAL_SIZE; }

    Literal const& operator[](std::size_t i) const;
    Literal const& at(std::size_t i) const;
};

} // namespace Interpretation

#endif // INTERPRETER_LITERALS_H
