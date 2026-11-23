#pragma once

#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "ectype.h"
#include "implicit_exceptions.h"

namespace Interpretation {

using CastOp         = Cbc::Format::CastOp;
using CheckedCastOp  = Cbc::Format::CheckedCastOp;
using CastType       = Cbc::Format::CastType;
using UncheckedCasts = Cbc::Format::UncheckedCasts;
using CheckedCasts   = Cbc::Format::CheckedCasts;

template <typename T> inline Value::Primitive MakeIntPrim(T value)
{
    static_assert(std::is_integral_v<T>, "integral value is expected");
    return Value::Primitive { .u64 = static_cast<uint64_t>(value) };
}

template <typename T> inline Value::Primitive MakeFpPrim(T value)
{
    static_assert(std::is_floating_point_v<T> || std::is_same_v<T, Utils::Float16>, "floating point value is expected");
    Value::Primitive res = { .u64 = 0 };
    if constexpr (std::is_same_v<T, Utils::Float16>) {
        res.f16 = value;
    } else if constexpr (std::is_same_v<T, float>) {
        res.f32 = value;
    } else {
        res.f64 = value;
    }
    return res;
}

template <typename To, typename From> inline Value::Primitive CastInt(From value)
{
    using Narrow = std::conditional_t<(sizeof(From) < sizeof(To)), From, To>;
    return MakeIntPrim(static_cast<Narrow>(value));
}

template <typename To, typename From> inline Value::Primitive CastFpToInt(From value)
{
    if (std::isnan(value)) {
        return MakeIntPrim(static_cast<To>(0));
    }
    if (value >= static_cast<From>(std::numeric_limits<To>::max())) {
        return MakeIntPrim(std::numeric_limits<To>::max());
    }
    if (value <= static_cast<From>(std::numeric_limits<To>::min())) {
        return MakeIntPrim(std::numeric_limits<To>::min());
    }
    return MakeIntPrim(static_cast<To>(value));
}

/// C++ type that backs the CBC type code in `Value::Primitive`.
template <CastType::Code Code> struct CppType {
    static_assert(Code < CastType::COUNT, "Not supported yet");
    // clang-format off
    using Type = std::conditional_t<Code == CastType::I8, int8_t,
        std::conditional_t<Code == CastType::U8, uint8_t,
        std::conditional_t<Code == CastType::I16, int16_t,
        std::conditional_t<Code == CastType::U16, uint16_t,
        std::conditional_t<Code == CastType::I32, int32_t,
        std::conditional_t<Code == CastType::U32, uint32_t,
        std::conditional_t<Code == CastType::I64, int64_t,
        std::conditional_t<Code == CastType::U64, uint64_t,
        std::conditional_t<Code == CastType::F16, Utils::Float16,
        std::conditional_t<Code == CastType::F32, float,
        std::conditional_t<Code == CastType::F64, double, void>>>>>>>>>>>;
    // clang-format on
};

template <CastType::Code Code> using CppTypeT = typename CppType<Code>::Type;

template <typename T> inline T PrimAs(Value::Primitive val)
{
    if constexpr (std::is_same_v<T, Utils::Float16>) {
        return val.f16;
    } else if constexpr (std::is_same_v<T, float>) {
        return val.f32;
    } else if constexpr (std::is_same_v<T, double>) {
        return val.f64;
    } else {
        return static_cast<T>(val.u64);
    }
}

template <CastType::Code From, CastType::Code To> inline Value::Primitive CastTyped(Value::Primitive val)
{
    using CppFrom = CppTypeT<From>;
    using CppTo   = CppTypeT<To>;
    const CppFrom from = PrimAs<CppFrom>(val);

    if constexpr (std::is_integral_v<CppFrom> && std::is_integral_v<CppTo>) {
        return CastInt<CppTo>(from);
    } else if constexpr (std::is_integral_v<CppTo>) {
        // Widen F16 exactly before comparing against integer bounds.
        using Wide = std::conditional_t<std::is_same_v<CppFrom, Utils::Float16>, float, CppFrom>;
        return CastFpToInt<CppTo>(static_cast<Wide>(from));
    } else if constexpr (std::is_same_v<CppFrom, Utils::Float16>) {
        return MakeFpPrim(static_cast<CppTo>(from));
    } else if constexpr (std::is_same_v<CppTo, Utils::Float16>) {
        if constexpr (std::is_same_v<CppFrom, double>) {
            return MakeFpPrim(Utils::F64toF16(from));
        } else {
            return MakeFpPrim(static_cast<Utils::Float16>(from));
        }
    } else {
        return MakeFpPrim(static_cast<CppTo>(from));
    }
}

using CastFn = Value::Primitive (*)(Value::Primitive);

template <typename List, size_t... Indices>
constexpr std::array<CastFn, sizeof...(Indices)> MakeCastFns(std::index_sequence<Indices...>)
{
    return { &CastTyped<List::pairs[Indices].from, List::pairs[Indices].to>... };
}

template <typename List>
inline constexpr auto CastFns = MakeCastFns<List>(std::make_index_sequence<Format::CastCode<List>::COUNT> {});

template <typename Op, auto& Fns> inline auto CastByOp(Op op, Value::Primitive val)
{
    if (!op.IsValid()) {
        FATAL("invalid cast op: %u", op.Raw());
    }
    return Fns[op.Raw()](val);
}

inline Value::Primitive CastPrim(CastOp op, Value::Primitive val)
{
    return CastByOp<CastOp, CastFns<UncheckedCasts>>(op, val);
}

struct CheckedCastResult {
    Value::Primitive value;
    Type exception;
};

// Exceptions thrown when a checked cast to `To` overflows. Their messages
// must match `tools/launcher/cbcengine-helper.cj`.
template <typename To> struct CheckedCastOverflow;

template <> struct CheckedCastOverflow<int8_t> {
    static constexpr Type minException = Type::CastOverflowI8Min;
    static constexpr Type maxException = Type::CastOverflowI8Max;
};

template <> struct CheckedCastOverflow<uint8_t> {
    static constexpr Type minException = Type::CastOverflowU8Min;
    static constexpr Type maxException = Type::CastOverflowU8Max;
};

template <> struct CheckedCastOverflow<int16_t> {
    static constexpr Type minException = Type::CastOverflowI16Min;
    static constexpr Type maxException = Type::CastOverflowI16Max;
};

template <> struct CheckedCastOverflow<uint16_t> {
    static constexpr Type minException = Type::CastOverflowU16Min;
    static constexpr Type maxException = Type::CastOverflowU16Max;
};

template <> struct CheckedCastOverflow<int32_t> {
    static constexpr Type minException = Type::CastOverflowI32Min;
    static constexpr Type maxException = Type::CastOverflowI32Max;
};

template <> struct CheckedCastOverflow<uint32_t> {
    static constexpr Type minException = Type::CastOverflowU32Min;
    static constexpr Type maxException = Type::CastOverflowU32Max;
};

template <> struct CheckedCastOverflow<int64_t> {
    static constexpr Type minException = Type::CastOverflowI64Min;
    static constexpr Type maxException = Type::CastOverflowI64Max;
};

template <> struct CheckedCastOverflow<uint64_t> {
    static constexpr Type minException = Type::CastOverflowU64Min;
    static constexpr Type maxException = Type::CastOverflowU64Max;
};

template <typename To> inline CheckedCastResult CheckedCastOverflowed(Type exception)
{
    return { MakeIntPrim<To>(0), exception };
}

inline CheckedCastResult CheckedCastSucceeded(Value::Primitive value)
{
    return { value, Type::Successful };
}

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> static inline CheckedCastResult CheckedCastFromSigned(int64_t value)
{
    // Currently only checked casts to integral types are supported
    static_assert(std::is_integral_v<To>, "integral target is expected");

    if constexpr (std::is_signed_v<To>) {
        if (value < static_cast<int64_t>(std::numeric_limits<To>::min())) {
            return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::minException);
        }
        if (value > static_cast<int64_t>(std::numeric_limits<To>::max())) {
            return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::maxException);
        }
    } else {
        if (value < 0) {
            return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::minException);
        }
        // int64_t is always representable in uint64_t, so only narrower
        // unsigned targets need an upper bound check.
        if constexpr (sizeof(To) < sizeof(uint64_t)) {
            if (static_cast<uint64_t>(value) > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
                return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::maxException);
            }
        }
    }
    return CheckedCastSucceeded(MakeIntPrim(static_cast<To>(value)));
}

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> static inline CheckedCastResult CheckedCastFromUnsigned(uint64_t value)
{
    // Currently only checked casts to integral types are supported
    static_assert(std::is_integral_v<To>, "integral target is expected");
    if constexpr (std::is_signed_v<To>) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::maxException);
        }
    } else if constexpr (sizeof(To) < sizeof(uint64_t)) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::maxException);
        }
    }
    return CheckedCastSucceeded(MakeIntPrim(static_cast<To>(value)));
}

// Bounds of an integral type encoded in double, as a half-open interval
// [lower, upper). A truncated float is representable in `To` iff it lies in
// that interval. Both bounds are exact powers of two; `upper` itself is not
// representable in `To` (it is one past the maximum value).
template <typename To> struct FpIntBounds {
    static_assert(std::is_integral_v<To>, "integral type is expected");
    static_assert(std::numeric_limits<To>::digits <= 64, "integer type is too wide");

    static constexpr bool isSigned = std::is_signed_v<To>;

    // 2^digits. Written as (1 << (digits - 1)) * 2.0 to elide std::ldexp and to
    // avoid a shift by 64, which is undefined for uint64_t.
    static constexpr double upper =
        static_cast<double>(1ULL << (std::numeric_limits<To>::digits - 1)) * 2.0;
    static constexpr double lower = isSigned ? -upper : 0.0;
};

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> inline CheckedCastResult CheckedCastFromFp(double value)
{
    static_assert(std::is_integral_v<To>, "integral target is expected");

    // NaN and infinities are never representable in an integer type.
    if (!std::isfinite(value)) {
        return CheckedCastOverflowed<To>(Type::CastOverflowInfOrNaN);
    }

    // Rounding is truncation toward zero; the range check must be applied to the
    // truncated value (so e.g. -128.9 fits into Int8 as -128).
    const double truncated = std::trunc(value);

    if (truncated < FpIntBounds<To>::lower) {
        return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::minException);
    }
    if (truncated >= FpIntBounds<To>::upper) {
        return CheckedCastOverflowed<To>(CheckedCastOverflow<To>::maxException);
    }
    return CheckedCastSucceeded(MakeIntPrim(static_cast<To>(truncated)));
}

template <CastType::Code From, CastType::Code To> inline CheckedCastResult CheckedCastTyped(Value::Primitive val)
{
    static_assert(std::is_integral_v<CppTypeT<To>>, "checked cast target must be integral");
    using CppFrom = CppTypeT<From>;
    const CppFrom from = PrimAs<CppFrom>(val);
    if constexpr (std::is_integral_v<CppFrom>) {
        if constexpr (std::is_signed_v<CppFrom>) {
            return CheckedCastFromSigned<CppTypeT<To>>(from);
        } else {
            return CheckedCastFromUnsigned<CppTypeT<To>>(from);
        }
    } else {
        return CheckedCastFromFp<CppTypeT<To>>(static_cast<double>(from));
    }
}

using CheckedCastFn = CheckedCastResult (*)(Value::Primitive);

template <typename List, size_t... Indices>
constexpr std::array<CheckedCastFn, sizeof...(Indices)> MakeCheckedCastFns(std::index_sequence<Indices...>)
{
    return { &CheckedCastTyped<List::pairs[Indices].from, List::pairs[Indices].to>... };
}

template <typename List>
inline constexpr auto CheckedCastFns =
    MakeCheckedCastFns<List>(std::make_index_sequence<Format::CastCode<List>::COUNT> {});

inline CheckedCastResult CheckedCastPrim(CheckedCastOp op, Value::Primitive val)
{
    return CastByOp<CheckedCastOp, CheckedCastFns<CheckedCasts>>(op, val);
}

} // namespace Interpretation
