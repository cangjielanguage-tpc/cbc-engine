#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "cbc/isa.h"
#include "ectype.h"
#include "implicit_exceptions.h"

namespace Interpretation {

using ConvertType = Cbc::Format::ConvertType;

inline Value::Primitive UnsupportedCastFrom(ConvertType ct)
{
    ASSERTION(false, "unsupported cast: from %s)", ct.ToStr().data());
    return Value::Primitive { .u64 = 0 };
}

inline Value::Primitive UnsupportedCastTo(ConvertType ct)
{
    ASSERTION(false, "unsupported cast: to %s)", ct.ToStr().data());
    return Value::Primitive { .u64 = 0 };
}

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

template <typename From> inline Value::Primitive CastFrom32(ConvertType toType, From value)
{
    switch (toType) {
        case ConvertType::F16: return MakeFpPrim(static_cast<Utils::Float16>(value));
        case ConvertType::I8:  return CastInt<int8_t>(value);
        case ConvertType::U8:  return CastInt<uint8_t>(value);
        case ConvertType::I16: return CastInt<int16_t>(value);
        case ConvertType::U16: return CastInt<uint16_t>(value);
        case ConvertType::I64: return CastInt<int64_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

inline Value::Primitive CastFromI64(ConvertType toType, int64_t value)
{
    switch (toType) {
        case ConvertType::F16: return MakeFpPrim(static_cast<Utils::Float16>(value));
        case ConvertType::I32: return CastInt<int32_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

inline Value::Primitive CastFromU64(ConvertType toType, uint64_t value)
{
    switch (toType) {
        case ConvertType::F16: return MakeFpPrim(static_cast<Utils::Float16>(value));
        case ConvertType::I32: return CastInt<int32_t>(value);
        case ConvertType::U32: return CastInt<uint32_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

template <typename From> inline Value::Primitive CastFloatingToIntegral(ConvertType toType, From value)
{
    switch (toType) {
        case ConvertType::I8:  return CastFpToInt<int8_t>(value);
        case ConvertType::U8:  return CastFpToInt<uint8_t>(value);
        case ConvertType::I16: return CastFpToInt<int16_t>(value);
        case ConvertType::U16: return CastFpToInt<uint16_t>(value);
        case ConvertType::I32: return CastFpToInt<int32_t>(value);
        case ConvertType::U32: return CastFpToInt<uint32_t>(value);
        case ConvertType::I64: return CastFpToInt<int64_t>(value);
        case ConvertType::U64: return CastFpToInt<uint64_t>(value);
        default:               return UnsupportedCastTo(toType);
    }
}

inline Value::Primitive CastFromF16(ConvertType toType, Utils::Float16 value)
{
    switch (toType) {
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:
            // widen exactly before comparing against integer bounds
            return CastFloatingToIntegral(toType, static_cast<float>(value));
    }
}

inline Value::Primitive CastFromF32(ConvertType toType, float value)
{
    switch (toType) {
        case ConvertType::F16: return MakeFpPrim(static_cast<Utils::Float16>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return CastFloatingToIntegral(toType, value);
    }
}

inline Value::Primitive CastFromF64(ConvertType toType, double value)
{
    switch (toType) {
        case ConvertType::F16: return MakeFpPrim(static_cast<Utils::Float16>(value));
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        default:               return CastFloatingToIntegral(toType, value);
    }
}

inline Value::Primitive CastPrim(ConvertType toType, ConvertType fromType, Value::Primitive val)
{
    switch (fromType) {
        case ConvertType::I32: return CastFrom32(toType, static_cast<int32_t>(val.u32));
        case ConvertType::U32: return CastFrom32(toType, val.u32);
        case ConvertType::I64: return CastFromI64(toType, static_cast<int64_t>(val.u64));
        case ConvertType::U64: return CastFromU64(toType, val.u64);

        case ConvertType::F16: return CastFromF16(toType, val.f16);
        case ConvertType::F32: return CastFromF32(toType, val.f32);
        case ConvertType::F64: return CastFromF64(toType, val.f64);

        default: return UnsupportedCastFrom(fromType);
    }
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

inline CheckedCastResult CheckedCastFromSignedTo(ConvertType toType, int64_t value)
{
    switch (toType) {
        case ConvertType::I8:  return CheckedCastFromSigned<int8_t>(value);
        case ConvertType::U8:  return CheckedCastFromSigned<uint8_t>(value);
        case ConvertType::I16: return CheckedCastFromSigned<int16_t>(value);
        case ConvertType::U16: return CheckedCastFromSigned<uint16_t>(value);
        case ConvertType::I32: return CheckedCastFromSigned<int32_t>(value);
        case ConvertType::U32: return CheckedCastFromSigned<uint32_t>(value);
        case ConvertType::I64: return CheckedCastFromSigned<int64_t>(value);
        case ConvertType::U64: return CheckedCastFromSigned<uint64_t>(value);
        default:               return { UnsupportedCastTo(toType), Type::Successful };
    }
}

inline CheckedCastResult CheckedCastFromUnsignedTo(ConvertType toType, uint64_t value)
{
    switch (toType) {
        case ConvertType::I8:  return CheckedCastFromUnsigned<int8_t>(value);
        case ConvertType::U8:  return CheckedCastFromUnsigned<uint8_t>(value);
        case ConvertType::I16: return CheckedCastFromUnsigned<int16_t>(value);
        case ConvertType::U16: return CheckedCastFromUnsigned<uint16_t>(value);
        case ConvertType::I32: return CheckedCastFromUnsigned<int32_t>(value);
        case ConvertType::U32: return CheckedCastFromUnsigned<uint32_t>(value);
        case ConvertType::I64: return CheckedCastFromUnsigned<int64_t>(value);
        case ConvertType::U64: return CheckedCastFromUnsigned<uint64_t>(value);
        default:               return { UnsupportedCastTo(toType), Type::Successful };
    }
}

inline CheckedCastResult CheckedCastFromIntegral(ConvertType toType, ConvertType fromType, Value::Primitive val)
{
    switch (fromType) {
        case ConvertType::I8:  return CheckedCastFromSignedTo(toType, static_cast<int8_t>(val.u64));
        case ConvertType::U8:  return CheckedCastFromUnsignedTo(toType, static_cast<uint8_t>(val.u64));
        case ConvertType::I16: return CheckedCastFromSignedTo(toType, static_cast<int16_t>(val.u64));
        case ConvertType::U16: return CheckedCastFromUnsignedTo(toType, static_cast<uint16_t>(val.u64));
        case ConvertType::I32: return CheckedCastFromSignedTo(toType, static_cast<int32_t>(val.u32));
        case ConvertType::U32: return CheckedCastFromUnsignedTo(toType, val.u32);
        case ConvertType::I64: return CheckedCastFromSignedTo(toType, static_cast<int64_t>(val.u64));
        case ConvertType::U64: return CheckedCastFromUnsignedTo(toType, val.u64);
        default:               return { UnsupportedCastFrom(fromType), Type::Successful };
    }
}

inline CheckedCastResult CheckedCastFromFloating(ConvertType toType, double value)
{
    switch (toType) {
        case ConvertType::I8:  return CheckedCastFromFp<int8_t>(value);
        case ConvertType::U8:  return CheckedCastFromFp<uint8_t>(value);
        case ConvertType::I16: return CheckedCastFromFp<int16_t>(value);
        case ConvertType::U16: return CheckedCastFromFp<uint16_t>(value);
        case ConvertType::I32: return CheckedCastFromFp<int32_t>(value);
        case ConvertType::U32: return CheckedCastFromFp<uint32_t>(value);
        case ConvertType::I64: return CheckedCastFromFp<int64_t>(value);
        case ConvertType::U64: return CheckedCastFromFp<uint64_t>(value);
        default:               return { UnsupportedCastTo(toType), Type::Successful };
    }
}

inline CheckedCastResult CheckedCastPrim(ConvertType toType, ConvertType fromType, Value::Primitive val)
{
    // Only integer -> integer and float -> integer casts are checked. Anything
    // else never overflows, so it keeps the unchecked semantics.
    ASSERT(!toType.IsFloatingPoint());

    switch (fromType) {
        case ConvertType::F16: return CheckedCastFromFloating(toType, static_cast<double>(val.f16));
        case ConvertType::F32: return CheckedCastFromFloating(toType, val.f32);
        case ConvertType::F64: return CheckedCastFromFloating(toType, val.f64);
        default:               return CheckedCastFromIntegral(toType, fromType, val);
    }
}

} // namespace Interpretation
