#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cbc/isa.h"
#include "mock/interpreter.h"
#include "testutils.h"

enum class CheckedOp {
    Add,
    Sub,
    Mul,
    Div,
    UAdd,
    USub,
    UMul,
    Pow,
    Lsl,
    Lsr,
    Asr,
    SatDiv,
    SatMod,
    SatPow,
    SatULsr
};

enum class CheckedKind {
    Checked,
    Saturating
};

struct CheckedArithResult {
    uint64_t value;
    Interpretation::Type type;
};

// Mirrors WidthTraits::make: a signed T sign-extends into u32, an unsigned T
// zero-extends; the 32-bit pattern then widens to uint64_t by zero extension.
template <typename T> uint64_t CheckedMake(T value)
{
    if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return value;
    } else {
        return static_cast<uint32_t>(value);
    }
}

inline uint64_t CheckedSMax(int width) { return (uint64_t { 1 } << (width - 1)) - 1; }

inline uint64_t CheckedSMin(int width) { return uint64_t { 1 } << (width - 1); }

inline uint64_t CheckedUMax(int width) { return width == 64 ? ~uint64_t { 0 } : (uint64_t { 1 } << width) - 1; }

// Oracle for differential testing of the checked binary ops in
// src/interpreter/internal/operations.h. Intentionally implemented as an
// independent algorithm; used by the auto-tests only.
template <typename S, typename U> CheckedArithResult CheckedRefW(CheckedOp op, uint64_t l, uint64_t r)
{
    constexpr int64_t bitWidth = std::numeric_limits<U>::digits;
    S sl                       = static_cast<S>(l);
    S sr                       = static_cast<S>(r);
    U ul                       = static_cast<U>(l);
    U ur                       = static_cast<U>(r);
    S sres;
    U ures;
    switch (op) {
        case CheckedOp::Add:
            if (__builtin_add_overflow(sl, sr, &sres)) {
                return { 0, Interpretation::Type::AddOverflowException };
            }
            return { CheckedMake(sres), Interpretation::Type::Successful };
        case CheckedOp::Sub:
            if (__builtin_sub_overflow(sl, sr, &sres)) {
                return { 0, Interpretation::Type::SubOverflowException };
            }
            return { CheckedMake(sres), Interpretation::Type::Successful };
        case CheckedOp::Mul:
            if (__builtin_mul_overflow(sl, sr, &sres)) {
                return { 0, Interpretation::Type::MulOverflowException };
            }
            return { CheckedMake(sres), Interpretation::Type::Successful };
        case CheckedOp::Div:
            if (sr == 0) {
                return { 0, Interpretation::Type::ArithmeticException };
            }
            if (sl == std::numeric_limits<S>::min() && sr == -1) {
                return { 0, Interpretation::Type::DivOverflowException };
            }
            return { CheckedMake(static_cast<S>(sl / sr)), Interpretation::Type::Successful };
        case CheckedOp::UAdd:
            if (__builtin_add_overflow(ul, ur, &ures)) {
                return { 0, Interpretation::Type::AddOverflowException };
            }
            return { CheckedMake(ures), Interpretation::Type::Successful };
        case CheckedOp::USub:
            if (__builtin_sub_overflow(ul, ur, &ures)) {
                return { 0, Interpretation::Type::SubOverflowException };
            }
            return { CheckedMake(ures), Interpretation::Type::Successful };
        case CheckedOp::UMul:
            if (__builtin_mul_overflow(ul, ur, &ures)) {
                return { 0, Interpretation::Type::MulOverflowException };
            }
            return { CheckedMake(ures), Interpretation::Type::Successful };
        case CheckedOp::Pow: {
            S base     = sl;
            U exponent = ur;
            if (exponent == 0) {
                return { CheckedMake(static_cast<S>(1)), Interpretation::Type::Successful };
            }
            S result = 1;
            while (exponent != 0) {
                if (exponent & 1) {
                    if (__builtin_mul_overflow(result, base, &result)) {
                        return { 0, Interpretation::Type::PowOverflowException };
                    }
                }
                exponent >>= 1;
                if (exponent != 0) {
                    if (__builtin_mul_overflow(base, base, &base)) {
                        return { 0, Interpretation::Type::PowOverflowException };
                    }
                }
            }
            return { CheckedMake(result), Interpretation::Type::Successful };
        }
        case CheckedOp::Lsl:
            if (sr >= static_cast<S>(bitWidth)) {
                return { 0, Interpretation::Type::LshOverflowException };
            }
            if (sr < 0) {
                return { 0, Interpretation::Type::LshNegativeShift };
            }
            return { CheckedMake(static_cast<U>(ul << sr)), Interpretation::Type::Successful };
        case CheckedOp::Lsr:
            if (sr >= static_cast<S>(bitWidth)) {
                return { 0, Interpretation::Type::RshOverflowException };
            }
            if (sr < 0) {
                return { 0, Interpretation::Type::RshNegativeShift };
            }
            return { CheckedMake(static_cast<U>(ul >> sr)), Interpretation::Type::Successful };
        case CheckedOp::Asr:
            if (sr >= static_cast<S>(bitWidth)) {
                return { 0, Interpretation::Type::AshOverflowException };
            }
            if (sr < 0) {
                return { 0, Interpretation::Type::AshNegativeShift };
            }
            return { CheckedMake(static_cast<S>(sl >> sr)), Interpretation::Type::Successful };
        case CheckedOp::SatDiv:
            // Saturating div is NOT clamping div: only smin / -1 saturates to
            // smax (Saturating::SDIV in operations.h), everything else
            // is plain truncating division (smin/1 = smin is left as is).
            if (sr == -1 && sl == std::numeric_limits<S>::min()) {
                return { CheckedMake(std::numeric_limits<S>::max()), Interpretation::Type::Successful };
            }
            return { CheckedMake(static_cast<S>(sl / sr)), Interpretation::Type::Successful };
        case CheckedOp::SatMod:
            // Saturating::SMOD (operations.h): smin % -1 -> 0, else
            // plain truncated modulo.
            if (sr == -1 && sl == std::numeric_limits<S>::min()) {
                return { CheckedMake(static_cast<S>(0)), Interpretation::Type::Successful };
            }
            return { CheckedMake(static_cast<S>(sl % sr)), Interpretation::Type::Successful };
        case CheckedOp::SatPow: {
            // Saturating::SPOW (operations.h): signed base, unsigned
            // exponent; bases with |base| <= 1 are special-cased (0^0 = 1),
            // the rest multiplies a wide accumulator while it stays in range
            // and saturates by the sign of the escaped value, flipped by the
            // remaining exponent's parity for a negative base.
            using swide      = __int128;
            constexpr S smin = std::numeric_limits<S>::min();
            constexpr S smax = std::numeric_limits<S>::max();
            S base           = sl;
            U exp            = ur;
            if (base == 0) {
                return { CheckedMake(static_cast<S>(exp == 0 ? 1 : 0)), Interpretation::Type::Successful };
            }
            if (base == 1) {
                return { CheckedMake(static_cast<S>(1)), Interpretation::Type::Successful };
            }
            if (base == -1) {
                return { CheckedMake(static_cast<S>((exp & 1) != 0 ? -1 : 1)), Interpretation::Type::Successful };
            }
            swide acc = 1;
            while (exp != 0 && smin <= acc && acc <= smax) {
                acc *= base;
                exp--;
            }
            if (acc < smin || acc > smax) {
                bool negative = (acc < 0) != (base < 0 && (exp & 1) != 0);
                return { CheckedMake(negative ? smin : smax), Interpretation::Type::Successful };
            }
            return { CheckedMake(static_cast<S>(acc)), Interpretation::Type::Successful };
        }
        case CheckedOp::SatULsr:
            // Saturating::SULSR (operations.h): the shift is read
            // unsigned, cast to int64 and clamped to [0, bitWidth-1] - a
            // full-width shift pattern wraps to a negative int64 and clamps
            // to 0, a huge one clamps to bitWidth-1.
            {
                int64_t shift = static_cast<int64_t>(ur);
                if (shift < 0) {
                    shift = 0;
                }
                if (shift > bitWidth - 1) {
                    shift = bitWidth - 1;
                }
                return { CheckedMake(static_cast<U>(ul >> shift)), Interpretation::Type::Successful };
            }
    }
    return { 0, Interpretation::Type::Successful };
}

inline CheckedArithResult CheckedRef(CheckedOp op, int width, uint64_t l, uint64_t r)
{
    switch (width) {
        case 8:  return CheckedRefW<int8_t, uint8_t>(op, l, r);
        case 16: return CheckedRefW<int16_t, uint16_t>(op, l, r);
        case 32: return CheckedRefW<int32_t, uint32_t>(op, l, r);
        default: return CheckedRefW<int64_t, uint64_t>(op, l, r);
    }
}

inline constexpr uint64_t kCheckedArithSeed        = 0x20261112;
inline constexpr size_t kCheckedArithRandomSamples = 64;

inline std::vector<uint64_t> CheckedGridValues(int width)
{
    const uint64_t smin = CheckedSMin(width);
    const uint64_t smax = CheckedSMax(width);
    const uint64_t umax = CheckedUMax(width);
    const uint64_t w    = static_cast<uint64_t>(width);
    return { 0, 1, 2, w - 1, w, w + 1, smin, smin + 1, smax - 1, smax, umax - 1, umax };
}

inline std::vector<uint64_t> CheckedOperandValues(int width)
{
    if (width == 8) {
        std::vector<uint64_t> values;
        for (uint64_t v = 0; v <= CheckedUMax(width); ++v) {
            values.push_back(v);
        }
        return values;
    }
    std::vector<uint64_t> values = CheckedGridValues(width);
    std::mt19937_64 rng(kCheckedArithSeed);
    for (size_t i = 0; i < kCheckedArithRandomSamples; ++i) {
        values.push_back(rng());
    }
    return values;
}

struct CheckedOpInfo {
    CheckedOp op;
    CheckedKind kind;
    uint32_t opcode;
    const char* base;
    bool rr;
    bool imm;
    bool noZeroRhs;

    constexpr Cbc::Format::Checked checked() const { return Cbc::Format::Checked::Value(opcode); }

    constexpr Cbc::Format::Saturating saturating() const { return Cbc::Format::Saturating::Value(opcode); }
};

inline constexpr CheckedOpInfo kCheckedOps[] = {
    { .op        = CheckedOp::Add,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CADD,
      .base      = "cadd",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Sub,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CSUB,
      .base      = "csub",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Mul,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CMUL,
      .base      = "cmul",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Div,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CDIV,
      .base      = "cdiv",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::UAdd,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CUADD,
      .base      = "cuadd",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::USub,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CUSUB,
      .base      = "cusub",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::UMul,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CUMUL,
      .base      = "cumul",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Pow,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CPOW,
      .base      = "cpow",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Lsl,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CLSL,
      .base      = "clsl",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Lsr,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CLSR,
      .base      = "clsr",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::Asr,
      .kind      = CheckedKind::Checked,
      .opcode    = Cbc::Format::Checked::CASR,
      .base      = "casr",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::SatDiv,
      .kind      = CheckedKind::Saturating,
      .opcode    = Cbc::Format::Saturating::SDIV,
      .base      = "sdiv",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = true },
    { .op        = CheckedOp::SatMod,
      .kind      = CheckedKind::Saturating,
      .opcode    = Cbc::Format::Saturating::SMOD,
      .base      = "smod",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = true },
    { .op        = CheckedOp::SatPow,
      .kind      = CheckedKind::Saturating,
      .opcode    = Cbc::Format::Saturating::SPOW,
      .base      = "spow",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
    { .op        = CheckedOp::SatULsr,
      .kind      = CheckedKind::Saturating,
      .opcode    = Cbc::Format::Saturating::SULSR,
      .base      = "sulsr",
      .rr        = true,
      .imm       = true,
      .noZeroRhs = false },
};

inline constexpr Cbc::Format::Width kCheckedWidths[] = {
    Cbc::Format::Width::W8,
    Cbc::Format::Width::W16,
    Cbc::Format::Width::W32,
    Cbc::Format::Width::W64,
};

inline int CheckedWidthBits(Cbc::Format::Width w) { return static_cast<int>(w.NBits()); }

inline void ExpectCheckedMatch(
    CheckedOp op, int width, uint64_t lhs, uint64_t rhs, Interpretation::Type type, Interpretation::Value::Primitive res
)
{
    SCOPED_TRACE("lhs=" + hex(lhs) + " rhs=" + hex(rhs));
    CheckedArithResult ref = CheckedRef(op, width, lhs, rhs);
    if (ref.type != Interpretation::Type::Successful) {
        EXPECT_EQ(type, ref.type);
        return;
    }
    ASSERT_EQ(type, Interpretation::Type::Successful);
    if (width == 64) {
        EXPECT_EQ(res.u64, ref.value);
    } else {
        EXPECT_EQ(res.u32, static_cast<uint32_t>(ref.value));
    }
}

inline void ExpectCheckedRr(CheckedOp op, int width, uint64_t lhs, uint64_t rhs, Interpretation::Code code)
{
    Interpretation::Type type = InterpretThrows(code, U64(0), U64(lhs), U64(rhs));
    Interpretation::Value::Primitive res {};
    if (type == Interpretation::Type::Successful) {
        res = Interpret(code, U64(0), U64(lhs), U64(rhs));
    }
    ExpectCheckedMatch(op, width, lhs, rhs, type, res);
}

inline void ExpectCheckedImm(CheckedOp op, int width, uint64_t lhs, uint64_t rhs, Interpretation::Code code)
{
    Interpretation::Type type = InterpretThrows(code, U64(0), U64(lhs));
    Interpretation::Value::Primitive res {};
    if (type == Interpretation::Type::Successful) {
        res = Interpret(code, U64(0), U64(lhs));
    }
    ExpectCheckedMatch(op, width, lhs, rhs, type, res);
}

inline void ExpectCheckedAsmRr(CheckedOp op, int width, uint64_t lhs, uint64_t rhs, Interpretation::Code code)
{
    Interpretation::Type type = InterpretThrows(code, U64(lhs), U64(rhs));
    Interpretation::Value::Primitive res {};
    if (type == Interpretation::Type::Successful) {
        res = Interpret(code, U64(lhs), U64(rhs));
    }
    ExpectCheckedMatch(op, width, lhs, rhs, type, res);
}

inline void ExpectCheckedAsmImm(CheckedOp op, int width, uint64_t lhs, uint64_t rhs, Interpretation::Code code)
{
    Interpretation::Type type = InterpretThrows(code, U64(lhs), U64(0));
    Interpretation::Value::Primitive res {};
    if (type == Interpretation::Type::Successful) {
        res = Interpret(code, U64(lhs), U64(0));
    }
    ExpectCheckedMatch(op, width, lhs, rhs, type, res);
}
