#pragma once

#include <cstdint>
#include <type_traits>

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

namespace Detail {

// The register field classes a form reads as <Xn>/<Xd>/<Xm>: a general-purpose
// register or the zero register, of one width. The traits constrain operand
// types so a 32-bit register cannot be mixed into a 64-bit form and vice versa.
template <typename R> struct IsXRegOperand : std::false_type {};

template <> struct IsXRegOperand<XReg> : std::true_type {};

template <> struct IsXRegOperand<XzrReg> : std::true_type {};

template <typename R> struct IsWRegOperand : std::false_type {};

template <> struct IsWRegOperand<WReg> : std::true_type {};

template <> struct IsWRegOperand<WzrReg> : std::true_type {};

// The base register of a memory address: field 31 means SP there, never the
// zero register, so the zero register is not a base.
template <typename R> struct IsBaseReg : std::false_type {};

template <> struct IsBaseReg<XReg> : std::true_type {};

template <> struct IsBaseReg<SpReg> : std::true_type {};

constexpr uint8_t RegCode(XReg reg) { return reg.Code(); }

constexpr uint8_t RegCode(XzrReg) { return 31; }

constexpr uint8_t RegCode(WReg reg) { return reg.Code(); }

constexpr uint8_t RegCode(WzrReg) { return 31; }

constexpr uint8_t RegCode(SpReg) { return 31; }

} // namespace Detail

enum class Shift : uint8_t {
    Lsl,
    Lsr,
    Asr
};

// Tag selecting the shifted-by-12 form of unsigned 12-bit immediates.
struct Lsl12Tag {};

inline constexpr Lsl12Tag Lsl12 {};

// How a register-offset address extends its index register, in encoding order:
// a code's value is the one the encoders put in the option field. LSL and SXTX
// read a 64-bit index register, UXTW and SXTW a 32-bit one.
enum class IndexExtend : uint8_t {
    Uxtw = 2,
    Lsl  = 3,
    Sxtw = 6,
    Sxtx = 7
};

// Tag selecting the scaled register-offset form, in which the index register is
// shifted left by the base-2 logarithm of the access size so that it counts
// accessed elements rather than bytes. Without the tag the index is a byte
// offset.
struct ScaledTag {};

inline constexpr ScaledTag Scaled {};

// An address of the form [base, #imm]: a byte offset from a base register, which
// is a general register or SP. Whether the offset is scaled by the access size,
// and which values it may take, depends on the form that reads the address, so
// the form checks it.
class MemImm {
    uint8_t base_;
    int32_t offset_;

public:
    template <typename B, std::enable_if_t<Detail::IsBaseReg<B>::value, int> = 0>
    constexpr MemImm(B base, int32_t offset) : base_ { Detail::RegCode(base) },
                                               offset_ { offset }
    {}

    constexpr uint8_t Base() const { return base_; }

    constexpr int32_t Offset() const { return offset_; }
};

// An address of the form [base, index{, extend}{, scaled}]: a base register, a
// general-purpose register holding a byte or element index, the extension that
// reads that index (LSL/SXTX for a 64-bit index register, UXTW/SXTW for a
// 32-bit one) and whether it is scaled by the access size.
class MemIndex {
    uint8_t base_;
    uint8_t index_;
    IndexExtend extend_;
    bool scaled_;

public:
    template <typename B, typename I, std::enable_if_t<Detail::IsBaseReg<B>::value, int> = 0>
    constexpr MemIndex(B base, I index, IndexExtend extend, bool scaled)
        : base_ { Detail::RegCode(base) },
          index_ { Detail::RegCode(index) },
          extend_ { extend },
          scaled_ { scaled }
    {}

    constexpr uint8_t Base() const { return base_; }

    constexpr uint8_t Index() const { return index_; }

    constexpr IndexExtend Extend() const { return extend_; }

    constexpr bool IsScaled() const { return scaled_; }
};

// [base, #imm]
template <typename B, std::enable_if_t<Detail::IsBaseReg<B>::value, int> = 0>
constexpr MemImm Mem(B base, int32_t offset)
{
    return MemImm { base, offset };
}

// [base, index] and [base, index, <extend>]: the address of an unscaled
// register-offset access with a 64-bit index, extended as LSL (the default) or
// SXTX. The scaled forms take the same operands plus a trailing Scaled tag.
template <
    typename B,
    typename I,
    std::enable_if_t<Detail::IsBaseReg<B>::value && Detail::IsXRegOperand<I>::value, int> = 0>
constexpr MemIndex Mem(B base, I index, IndexExtend extend = IndexExtend::Lsl)
{
    ASSERT(extend == IndexExtend::Lsl || extend == IndexExtend::Sxtx);
    return MemIndex { base, index, extend, false };
}

template <
    typename B,
    typename I,
    std::enable_if_t<Detail::IsBaseReg<B>::value && Detail::IsXRegOperand<I>::value, int> = 0>
constexpr MemIndex Mem(B base, I index, ScaledTag)
{
    return MemIndex { base, index, IndexExtend::Lsl, true };
}

template <
    typename B,
    typename I,
    std::enable_if_t<Detail::IsBaseReg<B>::value && Detail::IsXRegOperand<I>::value, int> = 0>
constexpr MemIndex Mem(B base, I index, IndexExtend extend, ScaledTag)
{
    ASSERT(extend == IndexExtend::Lsl || extend == IndexExtend::Sxtx);
    return MemIndex { base, index, extend, true };
}

// The same for a 32-bit index register, which is only readable through UXTW or
// SXTW, so the extension is not optional.
template <
    typename B,
    typename I,
    std::enable_if_t<Detail::IsBaseReg<B>::value && Detail::IsWRegOperand<I>::value, int> = 0>
constexpr MemIndex Mem(B base, I index, IndexExtend extend)
{
    ASSERT(extend == IndexExtend::Uxtw || extend == IndexExtend::Sxtw);
    return MemIndex { base, index, extend, false };
}

template <
    typename B,
    typename I,
    std::enable_if_t<Detail::IsBaseReg<B>::value && Detail::IsWRegOperand<I>::value, int> = 0>
constexpr MemIndex Mem(B base, I index, IndexExtend extend, ScaledTag)
{
    ASSERT(extend == IndexExtend::Uxtw || extend == IndexExtend::Sxtw);
    return MemIndex { base, index, extend, true };
}

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
