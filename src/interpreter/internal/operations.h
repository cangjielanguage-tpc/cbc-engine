// is not supposed to be included anywhere outside interpreter.cpp

#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "interpreter/ectype.h"
#include "interpreter/literals.h"
#include "runtimesupport/runtime.h"
#include "utils/math.h"

namespace Interpretation {

using namespace Cbc::Format;

template <Width::Value width> struct WidthTraits;

template <> struct WidthTraits<Width::W32> {
    using utype = uint32_t;
    using stype = int32_t;

    static Value::Primitive make(utype value) { return Value::Primitive { .u32 = value }; }

    static Value::Primitive make(stype value) { return Value::Primitive { .u32 = static_cast<utype>(value) }; }

    static utype uget(Value::Primitive const& p) { return static_cast<utype>(p.u32); }

    static stype sget(Value::Primitive const& p) { return static_cast<stype>(p.u32); }
};

template <> struct WidthTraits<Width::W64> {
    using utype = uint64_t;
    using stype = int64_t;

    static Value::Primitive make(utype value) { return Value::Primitive { .u64 = value }; }

    static Value::Primitive make(stype value) { return Value::Primitive { .u64 = static_cast<utype>(value) }; }

    static utype uget(Value::Primitive const& p) { return static_cast<utype>(p.u64); }

    static stype sget(Value::Primitive const& p) { return static_cast<stype>(p.u64); }
};

template <> struct WidthTraits<Width::W16> {
    using utype = uint16_t;
    using stype = int16_t;

    static Value::Primitive make(utype value) { return Value::Primitive { .u32 = static_cast<uint32_t>(value) }; }

    static Value::Primitive make(stype value) { return Value::Primitive { .u32 = static_cast<uint32_t>(value) }; }

    static utype uget(Value::Primitive const& p) { return static_cast<utype>(p.u32); }

    static stype sget(Value::Primitive const& p) { return static_cast<stype>(p.u32); }
};

template <> struct WidthTraits<Width::W8> {
    using utype = uint8_t;
    using stype = int8_t;

    static Value::Primitive make(utype value) { return Value::Primitive { .u32 = static_cast<uint32_t>(value) }; }

    static Value::Primitive make(stype value) { return Value::Primitive { .u32 = static_cast<uint32_t>(value) }; }

    static utype uget(Value::Primitive const& p) { return static_cast<utype>(p.u32); }

    static stype sget(Value::Primitive const& p) { return static_cast<stype>(p.u32); }
};

struct ArithmeticResult {
    Value::Primitive result;
    bool successful;
};

// 128-bit integers. The saturating (and checked) arithmetic below relies on
// `__int128`/`unsigned __int128` and on `__builtin_*_overflow` builtins for
// wide-accumulate-and-clamp computations.
//
// Portability notes:
// - Architectures: 128-bit integer types are a compiler extension, not ISO
//   C++, but every 64-bit target this engine supports - x86_64, aarch64 and
//   riscv64 - provides them. On all three, 128-bit arithmetic is implemented
//   natively (two-register pairs), which is far cheaper than emulating wide
//   accumulate via 64-bit overflow flags.
// - Compilers: `__int128` and the `__builtin_{add,sub,mul}_overflow` family
//   are supported by GCC, Clang and ICC on 64-bit targets. MSVC is the notable
//   exception - it has neither `__int128` nor the overflow builtins; should
//   MSVC support ever be needed, the usages below would have to be guarded
//   (e.g. via `_mul128`/`_umul128` intrinsics and manual flag handling).
// - Alignment: `__int128` is 16-byte aligned on x86_64 and riscv64 but only
//   8-byte aligned on aarch64 (AAPCS64); only stack/local usage appears here,
//   so the difference is harmless.
// - Note that `__builtin_*_overflow` itself is not tied to 128-bit types: it
//   works for any integer width and is also used below with 64-bit operands
//   (e.g. CPOW), so a hypothetical port without `__int128` could still reuse
//   the 64-bit code paths but would need manual carry handling for the wide
//   saturating accumulators.

// Saturating arithmetic. Unlike the checked ops, saturating ops always produce
// a value (they clamp instead of raising an overflow exception), so the
// `successful` flag of the result is always true.
// A zero divisor is expected to be ruled out by the compiler (`divcheck`
// before `sdiv`/`smod`), so reaching it here is a compiler bug.
template <Width::Value width>
static inline ArithmeticResult SatArith(Saturating::Value op, Value::Primitive l, Value::Primitive r)
{
    using Traits = WidthTraits<width>;
    using utype  = typename Traits::utype;
    using stype  = typename Traits::stype;

    // Wide enough for any width × width product or sum without overflow.
    using swide = __int128;
    using uwide = unsigned __int128;

    constexpr stype smax = std::numeric_limits<stype>::max();
    constexpr stype smin = std::numeric_limits<stype>::min();
    constexpr utype umax = std::numeric_limits<utype>::max();

    auto makeS = [&](swide v) {
        return Traits::make(static_cast<stype>(v < smin ? smin : (v > smax ? smax : v)));
    };

    switch (op) {
        case Saturating::SADD: {
            return { makeS(static_cast<swide>(Traits::sget(l)) + static_cast<swide>(Traits::sget(r))), true };
        }
        case Saturating::SSUB: {
            return { makeS(static_cast<swide>(Traits::sget(l)) - static_cast<swide>(Traits::sget(r))), true };
        }
        case Saturating::SMUL: {
            return { makeS(static_cast<swide>(Traits::sget(l)) * static_cast<swide>(Traits::sget(r))), true };
        }
        case Saturating::SDIV: {
            stype left  = Traits::sget(l);
            stype right = Traits::sget(r);
            ASSERT(right != 0);
            if (right == -1 && left == smin) {
                return { Traits::make(smax), true };
            }
            return { Traits::make(static_cast<stype>(left / right)), true };
        }
        case Saturating::SMOD: {
            stype left  = Traits::sget(l);
            stype right = Traits::sget(r);
            ASSERT(right != 0);
            if (right == -1 && left == smin) {
                return { Traits::make(static_cast<stype>(0)), true };
            }
            return { Traits::make(static_cast<stype>(left % right)), true };
        }
        case Saturating::SPOW: {
            // Only signed base (Int64 ** UInt64) exists in Cangjie today.
            // Track the true result sign and saturate as soon as the exact
            // value no longer fits into the target type. Note that the base
            // is repeatedly squared, so it must be checked for overflow as
            // well (e.g. 2 ** 512 squares the base up to 2 ** 256).
            using swidex = __int128;
            swidex acc     = 1;
            swidex base    = static_cast<swidex>(Traits::sget(l));
            utype  exp     = Traits::uget(r);
            bool   overflow = false;
            int    sign    = 1; // sign of the true accumulated product
            while (exp != 0) {
                if ((exp & 1) != 0) {
                    // Multiplying by a negative base flips the product sign.
                    // (Comparing against acc's current sign would double-count
                    // flips already accumulated in `sign`.)
                    if (base < 0) {
                        sign = -sign;
                    }
                    if (overflow || __builtin_mul_overflow(acc, base, &acc)) {
                        overflow = true;
                        break;
                    }
                    if (acc > smax || acc < smin) {
                        overflow = true;
                        break;
                    }
                }
                exp >>= 1;
                if (exp != 0) {
                    if (__builtin_mul_overflow(base, base, &base)) {
                        overflow = true;
                        break;
                    }
                }
            }
            if (overflow) {
                // Once the exact value is out of range it can only grow,
                // so saturate in the direction of the true result sign.
                return { Traits::make(sign > 0 ? smax : smin), true };
            }
            return { Traits::make(static_cast<stype>(acc)), true };
        }
        case Saturating::SSHL: {
            // Provisional semantics until cjc defines saturating shifts:
            // out-of-range shift amounts are clamped to the bit width - 1.
            constexpr int64_t bitWidth = std::numeric_limits<utype>::digits;
            int64_t shift              = static_cast<int64_t>(Traits::sget(r));
            if (shift < 0) {
                shift = 0;
            }
            if (shift > bitWidth - 1) {
                shift = bitWidth - 1;
            }
            stype base = Traits::sget(l);
            swide res  = static_cast<swide>(base) << shift;
            return { makeS(res), true };
        }
        case Saturating::SSHR: {
            // Provisional semantics until cjc defines saturating shifts:
            // out-of-range shift amounts are clamped to the bit width - 1.
            constexpr int64_t bitWidth = std::numeric_limits<utype>::digits;
            int64_t shift              = static_cast<int64_t>(Traits::sget(r));
            if (shift < 0) {
                shift = 0;
            }
            if (shift > bitWidth - 1) {
                shift = bitWidth - 1;
            }
            return { Traits::make(static_cast<stype>(Traits::sget(l) >> shift)), true };
        }
        default:
            FATAL("Unexpected Saturating op: %d", static_cast<int>(op));
    }
}

// Unsigned saturating arithmetic.
template <Width::Value width>
static inline ArithmeticResult SatArithU(Saturating::Value op, Value::Primitive l, Value::Primitive r)
{
    using Traits = WidthTraits<width>;
    using utype  = typename Traits::utype;

    using uwide = unsigned __int128;

    constexpr utype umax = std::numeric_limits<utype>::max();

    switch (op) {
        case Saturating::SUADD: {
            uwide res = static_cast<uwide>(Traits::uget(l)) + static_cast<uwide>(Traits::uget(r));
            return { Traits::make(static_cast<utype>(res > umax ? umax : res)), true };
        }
        case Saturating::SUSUB: {
            uwide left  = Traits::uget(l);
            uwide right = Traits::uget(r);
            return { Traits::make(static_cast<utype>(left >= right ? left - right : 0)), true };
        }
        case Saturating::SUMUL: {
            uwide res = static_cast<uwide>(Traits::uget(l)) * static_cast<uwide>(Traits::uget(r));
            return { Traits::make(static_cast<utype>(res > umax ? umax : res)), true };
        }
        case Saturating::SUDIV: {
            utype left  = Traits::uget(l);
            utype right = Traits::uget(r);
            ASSERT(right != 0);
            return { Traits::make(static_cast<utype>(left / right)), true };
        }
        case Saturating::SUMOD: {
            utype left  = Traits::uget(l);
            utype right = Traits::uget(r);
            ASSERT(right != 0);
            return { Traits::make(static_cast<utype>(left % right)), true };
        }
        case Saturating::SUSHL: {
            constexpr int64_t bitWidth = std::numeric_limits<utype>::digits;
            int64_t shift              = static_cast<int64_t>(Traits::uget(r));
            if (shift < 0) {
                shift = 0;
            }
            if (shift > bitWidth - 1) {
                shift = bitWidth - 1;
            }
            uwide res = static_cast<uwide>(Traits::uget(l)) << shift;
            return { Traits::make(static_cast<utype>(res > umax ? umax : res)), true };
        }
        case Saturating::SUSHR: {
            constexpr int64_t bitWidth = std::numeric_limits<utype>::digits;
            int64_t shift              = static_cast<int64_t>(Traits::uget(r));
            if (shift < 0) {
                shift = 0;
            }
            if (shift > bitWidth - 1) {
                shift = bitWidth - 1;
            }
            return { Traits::make(static_cast<utype>(Traits::uget(l) >> shift)), true };
        }
        default:
            FATAL("Unexpected unsigned Saturating op: %d", static_cast<int>(op));
    }
}

// TODO: Generalize it for all widths (like checked ops).
template <Width::Value width>
static inline ArithmeticResult Arith(Common::Value op, Value::Primitive l, Value::Primitive r);

template <> inline ArithmeticResult Arith<Width::W64>(Common::Value op, Value::Primitive l, Value::Primitive r)
{
    using namespace Cbc::Format;
    switch (op) {
        case Common::ADD: return { Value::Primitive { .u64 = l.u64 + r.u64 }, true };
        case Common::SUB: return { Value::Primitive { .u64 = l.u64 - r.u64 }, true };
        case Common::MUL: return { Value::Primitive { .u64 = l.u64 * r.u64 }, true };
        case Common::AND: return { Value::Primitive { .u64 = l.u64 & r.u64 }, true };
        case Common::OR:  return { Value::Primitive { .u64 = l.u64 | r.u64 }, true };
        case Common::XOR: return { Value::Primitive { .u64 = l.u64 ^ r.u64 }, true };
        case Common::LSR: return { Value::Primitive { .u64 = l.u64 >> (r.u64 & 0x3F) }, true };
        case Common::LSL: return { Value::Primitive { .u64 = l.u64 << (r.u64 & 0x3F) }, true };
        case Common::POW: FATAL("Not implemented yet POWI 64");

        case Common::ASR: {
            int64_t left = static_cast<int64_t>(l.u64);
            return { Value::Primitive { .u64 = static_cast<uint64_t>(left >> (r.u64 & 0x3F)) }, true };
        }

        case Common::UDIV: {
            if (r.u64 == 0) {
                return { l, false };
            }
            return { Value::Primitive { .u64 = l.u64 / r.u64 }, true };
        }
        case Common::UREM: {
            if (r.u64 == 0) {
                return { l, false };
            }
            return { Value::Primitive { .u64 = l.u64 % r.u64 }, true };
        }

        case Common::SDIV: {
            int64_t left  = static_cast<int64_t>(l.u64);
            int64_t right = static_cast<int64_t>(r.u64);
            if (right == 0) {
                return { l, false };
            }
            if (left == INT64_MIN && right == -1) {
                return { l, true };
            }
            return { Value::Primitive { .u64 = static_cast<uint64_t>(left / right) }, true };
        }
        case Common::SREM: {
            int64_t left  = static_cast<int64_t>(l.u64);
            int64_t right = static_cast<int64_t>(r.u64);
            if (right == 0) {
                return { l, false };
            }
            if (left == INT64_MIN && right == -1) {
                return { Value::Primitive { .u64 = 0 }, true };
            }
            return { Value::Primitive { .u64 = static_cast<uint64_t>(left % right) }, true };
        }
    }
}

template <> inline ArithmeticResult Arith<Width::W32>(Common::Value op, Value::Primitive l, Value::Primitive r)
{
    using namespace Cbc::Format;
    switch (op) {
        case Common::ADD: return { Value::Primitive { .u32 = l.u32 + r.u32 }, true };
        case Common::SUB: return { Value::Primitive { .u32 = l.u32 - r.u32 }, true };
        case Common::MUL: return { Value::Primitive { .u32 = l.u32 * r.u32 }, true };
        case Common::AND: return { Value::Primitive { .u32 = l.u32 & r.u32 }, true };
        case Common::OR:  return { Value::Primitive { .u32 = l.u32 | r.u32 }, true };
        case Common::XOR: return { Value::Primitive { .u32 = l.u32 ^ r.u32 }, true };
        case Common::LSR: return { Value::Primitive { .u32 = l.u32 >> (r.u32 & 0x1F) }, true };
        case Common::LSL: return { Value::Primitive { .u32 = l.u32 << (r.u32 & 0x1F) }, true };
        case Common::POW: FATAL("Not implemented yet POWI 32");

        case Common::ASR: {
            int32_t left = static_cast<int32_t>(l.u32);
            return { Value::Primitive { .u32 = static_cast<uint32_t>(left >> (r.u32 & 0x1F)) }, true };
        }

        case Common::UDIV: {
            if (r.u32 == 0) {
                return { l, false };
            }
            return { Value::Primitive { .u32 = l.u32 / r.u32 }, true };
        }
        case Common::UREM: {
            if (r.u32 == 0) {
                return { l, false };
            }
            return { Value::Primitive { .u32 = l.u32 % r.u32 }, true };
        }

        case Common::SDIV: {
            int32_t left  = static_cast<int32_t>(l.u32);
            int32_t right = static_cast<int32_t>(r.u32);
            if (right == 0) {
                return { l, false };
            }
            if (left == INT32_MIN && right == -1) {
                return { l, true };
            }
            return { Value::Primitive { .u32 = static_cast<uint32_t>(left / right) }, true };
        }
        case Common::SREM: {
            int32_t left  = static_cast<int32_t>(l.u32);
            int32_t right = static_cast<int32_t>(r.u32);
            if (right == 0) {
                return { l, false };
            }
            if (left == INT32_MIN && right == -1) {
                return { Value::Primitive { .u32 = 0 }, true };
            }
            return { Value::Primitive { .u32 = static_cast<uint32_t>(left % right) }, true };
        }
    }
}

template <Width::Value width>
static inline ArithmeticResult Arith(Checked::Value op, Value::Primitive l, Value::Primitive r)
{
    using Traits = WidthTraits<width>;
    using utype  = typename Traits::utype;
    using stype  = typename Traits::stype;
    using namespace Cbc::Format;

    constexpr int64_t bitWidth = std::numeric_limits<utype>::digits;
    auto checkShift            = [&](int64_t shift) { return 0 <= shift && shift < bitWidth; };

    switch (op) {
        case Checked::CADD: {
            stype result;
            bool overflow = __builtin_add_overflow(Traits::sget(l), Traits::sget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CSUB: {
            stype result;
            bool overflow = __builtin_sub_overflow(Traits::sget(l), Traits::sget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CMUL: {
            stype result;
            bool overflow = __builtin_mul_overflow(Traits::sget(l), Traits::sget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CDIV: {
            stype left  = Traits::sget(l);
            stype right = Traits::sget(r);
            if (right == 0) {
                return { l, false };
            }
            if (left == std::numeric_limits<stype>::min() && right == -1) {
                return { l, false };
            }
            return { Traits::make(static_cast<stype>(left / right)), true };
        }
        case Checked::CUADD: {
            utype result;
            bool overflow = __builtin_add_overflow(Traits::uget(l), Traits::uget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CUSUB: {
            utype result;
            bool overflow = __builtin_sub_overflow(Traits::uget(l), Traits::uget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CUMUL: {
            utype result;
            bool overflow = __builtin_mul_overflow(Traits::uget(l), Traits::uget(r), &result);
            return { Traits::make(result), !overflow };
        }
        case Checked::CPOW: {
            stype base     = Traits::sget(l);
            utype exponent = Traits::uget(r);
            if (exponent == 0) {
                return { Traits::make(static_cast<stype>(1)), true };
            }

            stype result  = 1;
            bool overflow = false;
            while (exponent != 0) {
                if (exponent & 1) {
                    if (__builtin_mul_overflow(result, base, &result)) {
                        overflow = true;
                        break;
                    }
                }
                exponent >>= 1;
                if (exponent != 0) {
                    if (__builtin_mul_overflow(base, base, &base)) {
                        overflow = true;
                        break;
                    }
                }
            }
            return { Traits::make(result), !overflow };
        }
        case Checked::CLSH: {
            utype base  = Traits::uget(l);
            stype shift = Traits::sget(r);
            if (!checkShift(shift)) {
                return { l, false };
            }
            return { Traits::make(static_cast<utype>(base << shift)), true };
        }
        case Checked::CRSH: {
            utype base  = Traits::uget(l); // forces >> to be logical shift
            stype shift = Traits::sget(r);
            if (!checkShift(shift)) {
                return { l, false };
            }
            return { Traits::make(static_cast<utype>(base >> shift)), true };
        }
        case Checked::CASH: {
            stype base  = Traits::sget(l); // forces >> to be arith shift
            stype shift = Traits::sget(r);
            if (!checkShift(shift)) {
                return { l, false };
            }
            return { Traits::make(static_cast<stype>(base >> shift)), true };
        }
        default: FATAL("Unexpected Checked op: %d", op);
    }
}

// TODO: Generalize it for all widths (like checked ops).
template <Width::Value width>
static inline ArithmeticResult ArithFP(FloatOperations::Value op, Value::Primitive l, Value::Primitive r);

template <>
inline ArithmeticResult ArithFP<Width::W64>(FloatOperations::Value op, Value::Primitive l, Value::Primitive r)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatOperations::FADD: return { Value::Primitive { .f64 = l.f64 + r.f64 }, true };
        case FloatOperations::FSUB: return { Value::Primitive { .f64 = l.f64 - r.f64 }, true };
        case FloatOperations::FMUL: return { Value::Primitive { .f64 = l.f64 * r.f64 }, true };
        case FloatOperations::FDIV: return { Value::Primitive { .f64 = l.f64 / r.f64 }, true };
        case FloatOperations::FPOW: return { Value::Primitive { .f64 = std::pow(l.f64, r.f64) }, true };

        default: FATAL("Unexpected FP op: %d", op);
    }
}

template <>
inline ArithmeticResult ArithFP<Width::W32>(FloatOperations::Value op, Value::Primitive l, Value::Primitive r)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatOperations::FADD: return { Value::Primitive { .f32 = l.f32 + r.f32 }, true };
        case FloatOperations::FSUB: return { Value::Primitive { .f32 = l.f32 - r.f32 }, true };
        case FloatOperations::FMUL: return { Value::Primitive { .f32 = l.f32 * r.f32 }, true };
        case FloatOperations::FDIV: return { Value::Primitive { .f32 = l.f32 / r.f32 }, true };
        case FloatOperations::FPOW: return { Value::Primitive { .f32 = std::pow(l.f32, r.f32) }, true };

        default: FATAL("Unexpected FP op: %d", op);
    }
}

// TODO: Generalize it for all widths (like checked ops).
template <Width::Value width> static inline ArithmeticResult ArithFP(FloatOperations::Value op, Value::Primitive s);

template <> inline ArithmeticResult ArithFP<Width::W64>(FloatOperations::Value op, Value::Primitive s)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatOperations::FSQRT: return { Value::Primitive { .f64 = std::sqrt(s.f64) }, true };
        case FloatOperations::FABS:  return { Value::Primitive { .f64 = std::fabs(s.f64) }, true };
        case FloatOperations::FNEG:  return { Value::Primitive { .f64 = -s.f64 }, true };

        default: FATAL("Unexpected FP op: %d", op);
    }
}

template <> inline ArithmeticResult ArithFP<Width::W32>(FloatOperations::Value op, Value::Primitive s)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatOperations::FSQRT: return { Value::Primitive { .f32 = std::sqrt(s.f32) }, true };
        case FloatOperations::FABS:  return { Value::Primitive { .f32 = std::fabs(s.f32) }, true };
        case FloatOperations::FNEG:  return { Value::Primitive { .f32 = -s.f32 }, true };

        default: FATAL("Unexpected FP op: %d", op);
    }
}

template <Width::Value width> static inline ArithmeticResult ArithFPMath(FloatMathOp::Value op, Value::Primitive s);

template <> inline ArithmeticResult ArithFPMath<Width::W64>(FloatMathOp::Value op, Value::Primitive s)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatMathOp::SIN: return { Value::Primitive { .f64 = std::sin(s.f64) }, true };
        case FloatMathOp::COS: return { Value::Primitive { .f64 = std::cos(s.f64) }, true };

        default: FATAL("Unexpected FP math op: %d", op);
    }
}

template <> inline ArithmeticResult ArithFPMath<Width::W32>(FloatMathOp::Value op, Value::Primitive s)
{
    using namespace Cbc::Format;
    switch (op) {
        case FloatMathOp::SIN: return { Value::Primitive { .f32 = std::sin(s.f32) }, true };
        case FloatMathOp::COS: return { Value::Primitive { .f32 = std::cos(s.f32) }, true };

        default: FATAL("Unexpected FP math op: %d", op);
    }
}

template <CC::Value cc, Width::Value width> inline static bool Compare(Value::Primitive l, Value::Primitive r);

template <> inline bool Compare<CC::EQ, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 == r.u32; }

template <> inline bool Compare<CC::NE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 != r.u32; }

template <> inline bool Compare<CC::LT, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return static_cast<int32_t>(l.u32) < static_cast<int32_t>(r.u32);
}

template <> inline bool Compare<CC::GE, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return static_cast<int32_t>(l.u32) >= static_cast<int32_t>(r.u32);
}

template <> inline bool Compare<CC::ULT, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 < r.u32; }

template <> inline bool Compare<CC::UGE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.u32 >= r.u32; }

template <> inline bool Compare<CC::TESTZ, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return (l.u32 & r.u32) == 0;
}

template <> inline bool Compare<CC::TESTNZ, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return (l.u32 & r.u32) != 0;
}

template <> inline bool Compare<CC::EQ, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 == r.u64; }

template <> inline bool Compare<CC::NE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 != r.u64; }

template <> inline bool Compare<CC::LT, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return static_cast<int64_t>(l.u64) < static_cast<int64_t>(r.u64);
}

template <> inline bool Compare<CC::GE, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return static_cast<int64_t>(l.u64) >= static_cast<int64_t>(r.u64);
}

template <> inline bool Compare<CC::ULT, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 < r.u64; }

template <> inline bool Compare<CC::UGE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.u64 >= r.u64; }

template <> inline bool Compare<CC::TESTZ, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return (l.u64 & r.u64) == 0;
}

template <> inline bool Compare<CC::TESTNZ, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return (l.u64 & r.u64) != 0;
}

template <CC::Value cc, Width::Value width> inline static bool Compare(Value::Reference l, Value::Reference r)
{
    FATAL("Unreachable");
    return false;
}

template <> inline bool Compare<CC::REQ, Width::W64>(Value::Reference l, Value::Reference r)
{
    return l.value == r.value;
}

template <> inline bool Compare<CC::RNE, Width::W64>(Value::Reference l, Value::Reference r)
{
    return l.value != r.value;
}

template <> inline bool Compare<CC::FEQ, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.f32 == r.f32; }

template <> inline bool Compare<CC::FNE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.f32 != r.f32; }

template <> inline bool Compare<CC::FGE, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.f32 >= r.f32; }

template <> inline bool Compare<CC::FNGE, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return !(l.f32 >= r.f32);
}

template <> inline bool Compare<CC::FLT, Width::W32>(Value::Primitive l, Value::Primitive r) { return l.f32 < r.f32; }

template <> inline bool Compare<CC::FNLT, Width::W32>(Value::Primitive l, Value::Primitive r)
{
    return !(l.f32 < r.f32);
}

template <> inline bool Compare<CC::FEQ, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.f64 == r.f64; }

template <> inline bool Compare<CC::FNE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.f64 != r.f64; }

template <> inline bool Compare<CC::FGE, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.f64 >= r.f64; }

template <> inline bool Compare<CC::FNGE, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return !(l.f64 >= r.f64);
}

template <> inline bool Compare<CC::FLT, Width::W64>(Value::Primitive l, Value::Primitive r) { return l.f64 < r.f64; }

template <> inline bool Compare<CC::FNLT, Width::W64>(Value::Primitive l, Value::Primitive r)
{
    return !(l.f64 < r.f64);
}

template <RT::ImmKind::Value immKind> static inline uint64_t DecodeImmediate(LiteralTable* literals, uint16_t value);

template <> inline uint64_t DecodeImmediate<RT::ImmKind::VALUE>(LiteralTable* literals, uint16_t value)
{
    return MathUtils::SignExtend(static_cast<uint64_t>(value), 12);
}

template <> inline uint64_t DecodeImmediate<RT::ImmKind::LITERAL>(LiteralTable* literals, uint16_t value)
{
    return literals->at(value).u64;
}

class MemoryLocation {
public:
    inline MemoryLocation(uint64_t location) : base(reinterpret_cast<uint8_t*>(location)), offset(0) {}

    inline MemoryLocation(uint8_t* _base, size_t _offset) : base(_base), offset(_offset) {}

    inline MemoryLocation(uintptr_t _base, size_t _offset) : base(reinterpret_cast<uint8_t*>(_base)), offset(_offset) {}

    inline void StorePrim(StoreAccessKind::Value stk, Format::Reg src, Ectype* ectype);
    inline void StoreImm(StoreAccessKind::Value stk, uint64_t imm);
    inline void LoadPrim(LoadAccessKind::Value ldk, Format::Reg dst, Ectype* ectype);

    inline void StoreRef(Format::Reg src, Ectype* ectype);
    inline void LoadRef(Format::Reg dst, Ectype* ectype);
    inline void Lea(Format::Reg dst, Ectype* ectype);

private:
    template <typename P> inline void Store(Format::Reg src, Ectype* ectype);
    template <typename P> inline void StoreImm(uint64_t imm);
    template <typename P> inline void Load(Format::Reg dst, Ectype* ectype);

    uint8_t* base;
    size_t offset;
};

template <> inline void MemoryLocation::Store<float>(Format::Reg src, Ectype* ectype)
{
    *reinterpret_cast<float*>(base + offset) = static_cast<float>(ectype->GetPrimitive(src.FR()).f32);
}

template <> inline void MemoryLocation::Store<double>(Format::Reg src, Ectype* ectype)
{
    *reinterpret_cast<double*>(base + offset) = static_cast<double>(ectype->GetPrimitive(src.FR()).f64);
}

template <> inline void MemoryLocation::Store<Value::Reference>(Format::Reg src, Ectype* ectype)
{
    *reinterpret_cast<uintptr_t*>(base + offset) = ectype->GetReference(src.IR()).value;
}

template <> inline void MemoryLocation::Load<float>(Format::Reg dst, Ectype* ectype)
{
    auto value = *reinterpret_cast<float*>(base + offset);
    ectype->Put(dst.FR(), Value::Primitive { .f32 = value });
}

template <> inline void MemoryLocation::Load<double>(Format::Reg dst, Ectype* ectype)
{
    auto value = *reinterpret_cast<double*>(base + offset);
    ectype->Put(dst.FR(), Value::Primitive { .f64 = value });
}

template <> inline void MemoryLocation::Load<Value::Reference>(Format::Reg dst, Ectype* ectype)
{
    auto value = *reinterpret_cast<uintptr_t*>(base + offset);
    ectype->Put(dst.IR(), Value::Reference { .value = value });
}

inline void MemoryLocation::Lea(Format::Reg dst, Ectype* ectype)
{
    auto value = reinterpret_cast<uintptr_t>(base + offset);
    ectype->Put(dst.IR(), Value::Reference { .value = value });
}

template <typename P> inline void MemoryLocation::Store(Format::Reg src, Ectype* ectype)
{
    *reinterpret_cast<P*>(base + offset) = static_cast<P>(ectype->GetPrimitive(src.IR()).u64);
}

template <typename P> inline void MemoryLocation::StoreImm(uint64_t imm)
{
    *reinterpret_cast<P*>(base + offset) = static_cast<P>(imm);
}

template <typename P> inline void MemoryLocation::Load(Format::Reg dst, Ectype* ectype)
{
    auto value = *reinterpret_cast<P*>(base + offset);
    ectype->Put(dst.IR(), Value::Primitive { .u64 = static_cast<uint64_t>(value) });
}

inline void MemoryLocation::StorePrim(StoreAccessKind::Value stk, Format::Reg src, Ectype* ectype)
{
    switch (stk) {
        case StoreAccessKind::ST_8:   Store<uint8_t>(src, ectype); return;
        case StoreAccessKind::ST_16:  Store<uint16_t>(src, ectype); return;
        case StoreAccessKind::ST_32:  Store<uint32_t>(src, ectype); return;
        case StoreAccessKind::ST_64:  Store<uint64_t>(src, ectype); return;
        case StoreAccessKind::ST_F32: Store<float>(src, ectype); return;
        case StoreAccessKind::ST_F64: Store<double>(src, ectype); return;
        default:                      FATAL("Unexpected stk");
    }
}

inline void MemoryLocation::StoreImm(StoreAccessKind::Value stk, uint64_t imm)
{
    switch (stk) {
        case StoreAccessKind::ST_8:  StoreImm<uint8_t>(imm); return;
        case StoreAccessKind::ST_16: StoreImm<uint16_t>(imm); return;
        case StoreAccessKind::ST_32: StoreImm<uint32_t>(imm); return;
        case StoreAccessKind::ST_64: StoreImm<uint64_t>(imm); return;
        default:                     FATAL("Unexpected stk");
    }
}

inline void MemoryLocation::LoadPrim(LoadAccessKind::Value ldk, Format::Reg dst, Ectype* ectype)
{
    switch (ldk) {
        case LoadAccessKind::LD_U8:      Load<uint8_t>(dst, ectype); return;
        case LoadAccessKind::LD_U16:     Load<uint16_t>(dst, ectype); return;
        case LoadAccessKind::LD_32:      Load<uint32_t>(dst, ectype); return;
        case LoadAccessKind::LD_64:      Load<uint64_t>(dst, ectype); return;
        case LoadAccessKind::LD_S8:      Load<int8_t>(dst, ectype); return;
        case LoadAccessKind::LD_S16:     Load<int16_t>(dst, ectype); return;
        case LoadAccessKind::LD_S32TO64: Load<int32_t>(dst, ectype); return;
        case LoadAccessKind::LD_F32:     Load<float>(dst, ectype); return;
        case LoadAccessKind::LD_F64:     Load<double>(dst, ectype); return;
        case LoadAccessKind::LD_LEA:     Lea(dst, ectype); return;
        default:                         FATAL("Unexpected ldk");
    }
}

inline void MemoryLocation::StoreRef(Format::Reg src, Ectype* ectype) { Store<Value::Reference>(src, ectype); }

inline void MemoryLocation::LoadRef(Format::Reg dst, Ectype* ectype) { Load<Value::Reference>(dst, ectype); }

inline uint32_t CalcLoadArrayOffset(LoadAccessKind::Value ldk, IReg idx, Ectype* ectype)
{
    uint32_t elemSize;
    switch (ldk) {
        case LoadAccessKind::LD_S8:  // fallthrough
        case LoadAccessKind::LD_U8:  elemSize = 1; break;
        case LoadAccessKind::LD_S16: // fallthrough
        case LoadAccessKind::LD_U16: elemSize = 2; break;
        case LoadAccessKind::LD_F32: // fallthrough
        case LoadAccessKind::LD_32:  elemSize = 4; break;
        case LoadAccessKind::LD_F64: // fallthrough
        case LoadAccessKind::LD_REF: // fallthrough
        case LoadAccessKind::LD_64:  elemSize = 8; break;
        default:                     FATAL("Unexpected ldk");
    }
    return RTSupport::MetaInfo::ArrayBodyOffset() + ectype->GetPrimitive(idx).u32 * elemSize;
}

inline uint32_t CalcStoreArrayOffset(StoreAccessKind::Value stk, IReg idx, Ectype* ectype)
{
    uint32_t elemSize;
    switch (stk) {
        case StoreAccessKind::ST_8:   elemSize = 1; break;
        case StoreAccessKind::ST_16:  elemSize = 2; break;
        case StoreAccessKind::ST_F32: // fallthrough
        case StoreAccessKind::ST_32:  elemSize = 4; break;
        case StoreAccessKind::ST_REF: // fallthrough
        case StoreAccessKind::ST_F64: // fallthrough
        case StoreAccessKind::ST_64:  elemSize = 8; break;
        default:                      FATAL("Unexpected ldk");
    }
    return RTSupport::MetaInfo::ArrayBodyOffset() + ectype->GetPrimitive(idx).u32 * elemSize;
}

} // namespace Interpretation
