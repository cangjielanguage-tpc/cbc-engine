#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Asm {

namespace Detail {
struct Op;
}

// One encoded aarch64 instruction word. Words are produced by the encoders and
// consumed by storage; Bits() is the only way to read the underlying value.
class Insn {
public:
    constexpr uint32_t Bits() const { return bits_; }

private:
    friend struct Detail::Op;

    constexpr explicit Insn(uint32_t bits) : bits_ { bits } {}

    uint32_t bits_;
};

namespace Detail {

// A value occupying bits [Shift, Shift + Width) of an encoded word. The value is
// masked to the field width; debug builds additionally assert that it already
// fits.
template <uint32_t Shift, uint32_t Width> struct Field {
    static constexpr uint32_t Mask = (1u << Width) - 1;

    uint32_t value;

    constexpr explicit Field(uint32_t v) : value { v & Mask } { ASSERT((v & ~Mask) == 0); }
};

// An encoded word under construction: fixed template bits with operand fields
// ORed in at their positions.
struct Op {
    uint32_t bits;

    constexpr explicit Op(uint32_t base) : bits { base } {}

    constexpr Insn Finish() const { return Insn { bits }; }
};

template <uint32_t Shift, uint32_t Width> constexpr Op operator|(Op op, Field<Shift, Width> field)
{
    return Op { op.bits | (field.value << Shift) };
}

} // namespace Detail

} // namespace Asm
