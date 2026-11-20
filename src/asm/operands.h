#pragma once

#include <cstdint>

#include "utils/assertion.h"

namespace Asm {

// General-purpose integer register, x0..x30. Register field 31 is not part of
// this type: it means the zero register in some operand positions and SP in
// others, and those are the distinct operand types XzrReg and SpReg. A form
// that accepts the zero register therefore takes it as XzrReg and only then.
class XReg {
    uint8_t code_;

public:
    constexpr explicit XReg(uint8_t code) : code_ { code } { ASSERT(code <= 30); }

    constexpr uint8_t Code() const { return code_; }
};

// 32-bit view of a general-purpose integer register, w0..w30.
class WReg {
    uint8_t code_;

public:
    constexpr explicit WReg(uint8_t code) : code_ { code } { ASSERT(code <= 30); }

    constexpr uint8_t Code() const { return code_; }
};

// The zero register (xzr/wzr), read as register field 31. A distinct type
// because field 31 also means SP (SpReg); the zero register is legal only in
// operand positions whose form accepts it, so those take XzrReg/WzrReg.
struct XzrReg {};

struct WzrReg {};

// Stack pointer. A distinct type because code 31 means SP only in operand
// positions that accept it.
class SpReg {};

inline constexpr XzrReg Xzr {};
inline constexpr WzrReg Wzr {};
inline constexpr SpReg Sp {};

enum class Shift : uint8_t {
    Lsl,
    Lsr,
    Asr
};

// Tag selecting the shifted-by-12 form of unsigned 12-bit immediates.
struct Lsl12Tag {};

inline constexpr Lsl12Tag Lsl12 {};

// Condition codes, in encoding order: a code's value is the one the encoders put
// in the cond field.
enum class Cc : uint8_t {
    Eq,
    Ne,
    Hs,
    Lo,
    Mi,
    Pl,
    Vs,
    Vc,
    Hi,
    Ls,
    Ge,
    Lt,
    Gt,
    Le,
    Al,
    Nv
};

inline constexpr XReg X0 { 0 };
inline constexpr XReg X1 { 1 };
inline constexpr XReg X2 { 2 };
inline constexpr XReg X3 { 3 };
inline constexpr XReg X4 { 4 };
inline constexpr XReg X5 { 5 };
inline constexpr XReg X6 { 6 };
inline constexpr XReg X7 { 7 };
inline constexpr XReg X8 { 8 };
inline constexpr XReg X9 { 9 };
inline constexpr XReg X10 { 10 };
inline constexpr XReg X11 { 11 };
inline constexpr XReg X12 { 12 };
inline constexpr XReg X13 { 13 };
inline constexpr XReg X14 { 14 };
inline constexpr XReg X15 { 15 };
inline constexpr XReg X16 { 16 };
inline constexpr XReg X17 { 17 };
inline constexpr XReg X18 { 18 };
inline constexpr XReg X19 { 19 };
inline constexpr XReg X20 { 20 };
inline constexpr XReg X21 { 21 };
inline constexpr XReg X22 { 22 };
inline constexpr XReg X23 { 23 };
inline constexpr XReg X24 { 24 };
inline constexpr XReg X25 { 25 };
inline constexpr XReg X26 { 26 };
inline constexpr XReg X27 { 27 };
inline constexpr XReg X28 { 28 };
inline constexpr XReg X29 { 29 };
inline constexpr XReg X30 { 30 };

inline constexpr WReg W0 { 0 };
inline constexpr WReg W1 { 1 };
inline constexpr WReg W2 { 2 };
inline constexpr WReg W3 { 3 };
inline constexpr WReg W4 { 4 };
inline constexpr WReg W5 { 5 };
inline constexpr WReg W6 { 6 };
inline constexpr WReg W7 { 7 };
inline constexpr WReg W8 { 8 };
inline constexpr WReg W9 { 9 };
inline constexpr WReg W10 { 10 };
inline constexpr WReg W11 { 11 };
inline constexpr WReg W12 { 12 };
inline constexpr WReg W13 { 13 };
inline constexpr WReg W14 { 14 };
inline constexpr WReg W15 { 15 };
inline constexpr WReg W16 { 16 };
inline constexpr WReg W17 { 17 };
inline constexpr WReg W18 { 18 };
inline constexpr WReg W19 { 19 };
inline constexpr WReg W20 { 20 };
inline constexpr WReg W21 { 21 };
inline constexpr WReg W22 { 22 };
inline constexpr WReg W23 { 23 };
inline constexpr WReg W24 { 24 };
inline constexpr WReg W25 { 25 };
inline constexpr WReg W26 { 26 };
inline constexpr WReg W27 { 27 };
inline constexpr WReg W28 { 28 };
inline constexpr WReg W29 { 29 };
inline constexpr WReg W30 { 30 };

} // namespace Asm
