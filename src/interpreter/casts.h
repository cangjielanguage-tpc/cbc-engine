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
    static_assert(std::is_floating_point_v<T>, "floating point value is expected");
    Value::Primitive res = { .u64 = 0 };
    if constexpr (std::is_same_v<T, float>) {
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
    static_assert(Code != CastType::F16 && Code < CastType::COUNT, "Not supported yet");
    // clang-format off
    using Type = std::conditional_t<Code == CastType::I8, int8_t,
        std::conditional_t<Code == CastType::U8, uint8_t,
        std::conditional_t<Code == CastType::I16, int16_t,
        std::conditional_t<Code == CastType::U16, uint16_t,
        std::conditional_t<Code == CastType::I32, int32_t,
        std::conditional_t<Code == CastType::U32, uint32_t,
        std::conditional_t<Code == CastType::I64, int64_t,
        std::conditional_t<Code == CastType::U64, uint64_t,
        std::conditional_t<Code == CastType::F32, float, double>>>>>>>>>;
    // clang-format on
};

template <CastType::Code Code> using CppTypeT = typename CppType<Code>::Type;

template <typename T> inline T PrimAs(Value::Primitive val)
{
    if constexpr (std::is_same_v<T, float>) {
        return val.f32;
    } else if constexpr (std::is_same_v<T, double>) {
        return val.f64;
    } else {
        return static_cast<T>(val.u64);
    }
}

template <CastType::Code From, CastType::Code To> inline Value::Primitive CastTyped(Value::Primitive val)
{
    const CppTypeT<From> from = PrimAs<CppTypeT<From>>(val);
    if constexpr (std::is_integral_v<CppTypeT<From>> && std::is_integral_v<CppTypeT<To>>) {
        return CastInt<CppTypeT<To>>(from);
    } else if constexpr (std::is_integral_v<CppTypeT<To>>) {
        return CastFpToInt<CppTypeT<To>>(from);
    } else {
        return MakeFpPrim(static_cast<CppTypeT<To>>(from));
    }
}

template <typename List> inline constexpr size_t CastPairCount = std::size(List::pairs);

using CastFn = Value::Primitive (*)(Value::Primitive);

template <typename List, size_t... Indices>
constexpr std::array<CastFn, sizeof...(Indices)> MakeCastFns(std::index_sequence<Indices...>)
{
    return { &CastTyped<List::pairs[Indices].from, List::pairs[Indices].to>... };
}

template <typename List>
inline constexpr auto CastFns = MakeCastFns<List>(std::make_index_sequence<CastPairCount<List>> {});

inline Value::Primitive CastPrim(CastOp op, Value::Primitive val)
{
    if (!op.IsValid()) {
        FATAL("invalid cast op: %u", op.Raw());
    }
    return CastFns<UncheckedCasts>[op.Raw()](val);
}

struct CheckedCastResult {
    Value::Primitive value;
    bool overflow;
};

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> static inline CheckedCastResult CheckedCastFromSigned(int64_t value)
{
    // Currently only checked casts to integral types are supported
    static_assert(std::is_integral_v<To>, "integral target is expected");

    if constexpr (std::is_signed_v<To>) {
        if (value < static_cast<int64_t>(std::numeric_limits<To>::min()) ||
            value > static_cast<int64_t>(std::numeric_limits<To>::max())) {
            return { MakeIntPrim<To>(0), true };
        }
    } else {
        if (value < 0) {
            return { MakeIntPrim<To>(0), true };
        }
        // int64_t is always representable in uint64_t, so only narrower
        // unsigned targets need an upper bound check.
        if constexpr (sizeof(To) < sizeof(uint64_t)) {
            if (static_cast<uint64_t>(value) > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
                return { MakeIntPrim<To>(0), true };
            }
        }
    }
    return { MakeIntPrim(static_cast<To>(value)), false };
}

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> static inline CheckedCastResult CheckedCastFromUnsigned(uint64_t value)
{
    // Currently only checked casts to integral types are supported
    static_assert(std::is_integral_v<To>, "integral target is expected");

    if constexpr (std::is_signed_v<To> || sizeof(To) < sizeof(uint64_t)) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return { MakeIntPrim<To>(0), true };
        }
    }
    return { MakeIntPrim(static_cast<To>(value)), false };
}

// Bound of integer type encoded in double: one past the largest representable value,
// or minus the smallest one for signed types.
// Done this way to elide std::ldexp call.
template <typename To>
inline constexpr double FpIntBound = std::is_signed_v<To> ? -static_cast<double>(std::numeric_limits<To>::min())
                                                          : static_cast<double>(std::numeric_limits<To>::max()) + 1.0;

// Checked casts rules: the source value must be representable
// in the target type, otherwise the cast throws `OverflowException`.
template <typename To> inline CheckedCastResult CheckedCastFromFp(double value)
{
    static_assert(std::is_integral_v<To>, "integral target is expected");

    // NaN and infinities are never representable in an integer type.
    if (!std::isfinite(value)) {
        return { MakeIntPrim<To>(0), true };
    }

    // Rounding is truncation toward zero; the range check must be applied to the
    // truncated value (so e.g. -128.9 fits into Int8 as -128).
    const double truncated = std::trunc(value);

    constexpr double bound = FpIntBound<To>;
    bool inRange;
    if constexpr (std::is_signed_v<To>) {
        inRange = truncated >= -bound && truncated < bound;
    } else {
        inRange = truncated >= 0.0 && truncated < bound;
    }
    if (!inRange) {
        return { MakeIntPrim<To>(0), true };
    }
    return { MakeIntPrim(static_cast<To>(truncated)), false };
}

template <CastType::Code From, CastType::Code To> inline CheckedCastResult CheckedCastTyped(Value::Primitive val)
{
    static_assert(std::is_integral_v<CppTypeT<To>>, "checked cast target must be integral");
    const CppTypeT<From> from = PrimAs<CppTypeT<From>>(val);
    if constexpr (std::is_floating_point_v<CppTypeT<From>>) {
        return CheckedCastFromFp<CppTypeT<To>>(from);
    } else if constexpr (std::is_signed_v<CppTypeT<From>>) {
        return CheckedCastFromSigned<CppTypeT<To>>(from);
    } else {
        return CheckedCastFromUnsigned<CppTypeT<To>>(from);
    }
}

using CheckedCastFn = CheckedCastResult (*)(Value::Primitive);

template <typename List, size_t... Indices>
constexpr std::array<CheckedCastFn, sizeof...(Indices)> MakeCheckedCastFns(std::index_sequence<Indices...>)
{
    return { &CheckedCastTyped<List::pairs[Indices].from, List::pairs[Indices].to>... };
}

template <typename List>
inline constexpr auto CheckedCastFns = MakeCheckedCastFns<List>(std::make_index_sequence<CastPairCount<List>> {});

inline CheckedCastResult CheckedCastPrim(CheckedCastOp op, Value::Primitive val)
{
    if (!op.IsValid()) {
        FATAL("invalid cast op: %u", op.Raw());
    }
    return CheckedCastFns<CheckedCasts>[op.Raw()](val);
}

} // namespace Interpretation
