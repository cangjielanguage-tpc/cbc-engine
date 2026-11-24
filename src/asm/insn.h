#pragma once

#include <array>
#include <cstdint>

#include "utils/assertion.h"

namespace Asm {

namespace Detail {
struct Op;
}

// One encoded aarch64 instruction word, held as its four bytes in target byte
// order (little-endian). The encoders produce it and storage writes it out;
// Bytes() is the underlying representation and Bits() reconstructs the word for
// the consumers that reason about the encoding rather than the bytes.
class Insn {
public:
    static constexpr uint32_t Size = 4;

    constexpr uint32_t Bits() const
    {
        return static_cast<uint32_t>(bytes_[0]) | (static_cast<uint32_t>(bytes_[1]) << 8) |
               (static_cast<uint32_t>(bytes_[2]) << 16) | (static_cast<uint32_t>(bytes_[3]) << 24);
    }

    constexpr const std::array<uint8_t, Size>& Bytes() const { return bytes_; }

private:
    friend struct Detail::Op;

    constexpr explicit Insn(uint32_t bits)
        : bytes_ { static_cast<uint8_t>(bits),
                   static_cast<uint8_t>(bits >> 8),
                   static_cast<uint8_t>(bits >> 16),
                   static_cast<uint8_t>(bits >> 24) }
    {}

    std::array<uint8_t, Size> bytes_;
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
