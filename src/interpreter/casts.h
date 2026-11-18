#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "ectype.h"

namespace Interpretation {

using CastOp        = Cbc::Format::CastOp;
using CheckedCastOp = Cbc::Format::CheckedCastOp;
using CastType      = Cbc::Format::CastType;

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

inline Value::Primitive CastPrim(CastOp op, Value::Primitive val)
{
    switch (op) {
        case CastOp::Make(CastType::I32, CastType::I8).Raw():  return CastInt<int8_t>(static_cast<int32_t>(val.u32));
        case CastOp::Make(CastType::I32, CastType::U8).Raw():  return CastInt<uint8_t>(static_cast<int32_t>(val.u32));
        case CastOp::Make(CastType::I32, CastType::I16).Raw(): return CastInt<int16_t>(static_cast<int32_t>(val.u32));
        case CastOp::Make(CastType::I32, CastType::U16).Raw(): return CastInt<uint16_t>(static_cast<int32_t>(val.u32));
        case CastOp::Make(CastType::I32, CastType::I64).Raw(): return CastInt<int64_t>(static_cast<int32_t>(val.u32));
        case CastOp::Make(CastType::I32, CastType::F32).Raw():
            return MakeFpPrim(static_cast<float>(static_cast<int32_t>(val.u32)));
        case CastOp::Make(CastType::I32, CastType::F64).Raw():
            return MakeFpPrim(static_cast<double>(static_cast<int32_t>(val.u32)));

        case CastOp::Make(CastType::U32, CastType::I8).Raw():  return CastInt<int8_t>(val.u32);
        case CastOp::Make(CastType::U32, CastType::U8).Raw():  return CastInt<uint8_t>(val.u32);
        case CastOp::Make(CastType::U32, CastType::I16).Raw(): return CastInt<int16_t>(val.u32);
        case CastOp::Make(CastType::U32, CastType::U16).Raw(): return CastInt<uint16_t>(val.u32);
        case CastOp::Make(CastType::U32, CastType::I64).Raw(): return CastInt<int64_t>(val.u32);
        case CastOp::Make(CastType::U32, CastType::F32).Raw(): return MakeFpPrim(static_cast<float>(val.u32));
        case CastOp::Make(CastType::U32, CastType::F64).Raw(): return MakeFpPrim(static_cast<double>(val.u32));

        case CastOp::Make(CastType::I64, CastType::I32).Raw(): return CastInt<int32_t>(static_cast<int64_t>(val.u64));
        case CastOp::Make(CastType::I64, CastType::F32).Raw():
            return MakeFpPrim(static_cast<float>(static_cast<int64_t>(val.u64)));
        case CastOp::Make(CastType::I64, CastType::F64).Raw():
            return MakeFpPrim(static_cast<double>(static_cast<int64_t>(val.u64)));

        case CastOp::Make(CastType::U64, CastType::I32).Raw(): return CastInt<int32_t>(val.u64);
        case CastOp::Make(CastType::U64, CastType::U32).Raw(): return CastInt<uint32_t>(val.u64);
        case CastOp::Make(CastType::U64, CastType::F32).Raw(): return MakeFpPrim(static_cast<float>(val.u64));
        case CastOp::Make(CastType::U64, CastType::F64).Raw(): return MakeFpPrim(static_cast<double>(val.u64));

        case CastOp::Make(CastType::F32, CastType::I8).Raw():  return CastFpToInt<int8_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::U8).Raw():  return CastFpToInt<uint8_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::I16).Raw(): return CastFpToInt<int16_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::U16).Raw(): return CastFpToInt<uint16_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::I32).Raw(): return CastFpToInt<int32_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::U32).Raw(): return CastFpToInt<uint32_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::I64).Raw(): return CastFpToInt<int64_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::U64).Raw(): return CastFpToInt<uint64_t>(val.f32);
        case CastOp::Make(CastType::F32, CastType::F64).Raw(): return MakeFpPrim(static_cast<double>(val.f32));

        case CastOp::Make(CastType::F64, CastType::I8).Raw():  return CastFpToInt<int8_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::U8).Raw():  return CastFpToInt<uint8_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::I16).Raw(): return CastFpToInt<int16_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::U16).Raw(): return CastFpToInt<uint16_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::I32).Raw(): return CastFpToInt<int32_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::U32).Raw(): return CastFpToInt<uint32_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::I64).Raw(): return CastFpToInt<int64_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::U64).Raw(): return CastFpToInt<uint64_t>(val.f64);
        case CastOp::Make(CastType::F64, CastType::F32).Raw(): return MakeFpPrim(static_cast<float>(val.f64));
    }
    FATAL("invalid cast op: %u", op.Raw());
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
    if constexpr (std::is_signed_v<To>) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return { MakeIntPrim<To>(0), true };
        }
    } else if constexpr (sizeof(To) < sizeof(uint64_t)) {
        if (value > static_cast<uint64_t>(std::numeric_limits<To>::max())) {
            return { MakeIntPrim<To>(0), true };
        }
    }
    return { MakeIntPrim(static_cast<To>(value)), false };
}

// Bound of integer type encoded in double.
// Done this way to elide std::ldexp call.
template <typename To> struct FpIntBound;

template <> struct FpIntBound<int8_t> {
    static constexpr double value = 0x1p7;
};

template <> struct FpIntBound<uint8_t> {
    static constexpr double value = 0x1p8;
};

template <> struct FpIntBound<int16_t> {
    static constexpr double value = 0x1p15;
};

template <> struct FpIntBound<uint16_t> {
    static constexpr double value = 0x1p16;
};

template <> struct FpIntBound<int32_t> {
    static constexpr double value = 0x1p31;
};

template <> struct FpIntBound<uint32_t> {
    static constexpr double value = 0x1p32;
};

template <> struct FpIntBound<int64_t> {
    static constexpr double value = 0x1p63;
};

template <> struct FpIntBound<uint64_t> {
    static constexpr double value = 0x1p64;
};

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

    constexpr double bound = FpIntBound<To>::value;
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

inline CheckedCastResult CheckedCastPrim(CheckedCastOp op, Value::Primitive val)
{
    switch (op) {
        case CheckedCastOp::Make(CastType::I8, CastType::U8).Raw():
            return CheckedCastFromSigned<uint8_t>(static_cast<int8_t>(val.u64));

        case CheckedCastOp::Make(CastType::U8, CastType::I8).Raw():
            return CheckedCastFromUnsigned<int8_t>(static_cast<uint8_t>(val.u64));

        case CheckedCastOp::Make(CastType::I16, CastType::I8).Raw():
            return CheckedCastFromSigned<int8_t>(static_cast<int16_t>(val.u64));
        case CheckedCastOp::Make(CastType::I16, CastType::U8).Raw():
            return CheckedCastFromSigned<uint8_t>(static_cast<int16_t>(val.u64));
        case CheckedCastOp::Make(CastType::I16, CastType::U16).Raw():
            return CheckedCastFromSigned<uint16_t>(static_cast<int16_t>(val.u64));

        case CheckedCastOp::Make(CastType::U16, CastType::I8).Raw():
            return CheckedCastFromUnsigned<int8_t>(static_cast<uint16_t>(val.u64));
        case CheckedCastOp::Make(CastType::U16, CastType::U8).Raw():
            return CheckedCastFromUnsigned<uint8_t>(static_cast<uint16_t>(val.u64));
        case CheckedCastOp::Make(CastType::U16, CastType::I16).Raw():
            return CheckedCastFromUnsigned<int16_t>(static_cast<uint16_t>(val.u64));

        case CheckedCastOp::Make(CastType::I32, CastType::I8).Raw():
            return CheckedCastFromSigned<int8_t>(static_cast<int32_t>(val.u32));
        case CheckedCastOp::Make(CastType::I32, CastType::U8).Raw():
            return CheckedCastFromSigned<uint8_t>(static_cast<int32_t>(val.u32));
        case CheckedCastOp::Make(CastType::I32, CastType::I16).Raw():
            return CheckedCastFromSigned<int16_t>(static_cast<int32_t>(val.u32));
        case CheckedCastOp::Make(CastType::I32, CastType::U16).Raw():
            return CheckedCastFromSigned<uint16_t>(static_cast<int32_t>(val.u32));
        case CheckedCastOp::Make(CastType::I32, CastType::U32).Raw():
            return CheckedCastFromSigned<uint32_t>(static_cast<int32_t>(val.u32));

        case CheckedCastOp::Make(CastType::U32, CastType::I8).Raw():  return CheckedCastFromUnsigned<int8_t>(val.u32);
        case CheckedCastOp::Make(CastType::U32, CastType::U8).Raw():  return CheckedCastFromUnsigned<uint8_t>(val.u32);
        case CheckedCastOp::Make(CastType::U32, CastType::I16).Raw(): return CheckedCastFromUnsigned<int16_t>(val.u32);
        case CheckedCastOp::Make(CastType::U32, CastType::U16).Raw(): return CheckedCastFromUnsigned<uint16_t>(val.u32);
        case CheckedCastOp::Make(CastType::U32, CastType::I32).Raw(): return CheckedCastFromUnsigned<int32_t>(val.u32);

        case CheckedCastOp::Make(CastType::I64, CastType::I8).Raw():
            return CheckedCastFromSigned<int8_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::U8).Raw():
            return CheckedCastFromSigned<uint8_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::I16).Raw():
            return CheckedCastFromSigned<int16_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::U16).Raw():
            return CheckedCastFromSigned<uint16_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::I32).Raw():
            return CheckedCastFromSigned<int32_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::U32).Raw():
            return CheckedCastFromSigned<uint32_t>(static_cast<int64_t>(val.u64));
        case CheckedCastOp::Make(CastType::I64, CastType::U64).Raw():
            return CheckedCastFromSigned<uint64_t>(static_cast<int64_t>(val.u64));

        case CheckedCastOp::Make(CastType::U64, CastType::I8).Raw():  return CheckedCastFromUnsigned<int8_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::U8).Raw():  return CheckedCastFromUnsigned<uint8_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::I16).Raw(): return CheckedCastFromUnsigned<int16_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::U16).Raw(): return CheckedCastFromUnsigned<uint16_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::I32).Raw(): return CheckedCastFromUnsigned<int32_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::U32).Raw(): return CheckedCastFromUnsigned<uint32_t>(val.u64);
        case CheckedCastOp::Make(CastType::U64, CastType::I64).Raw(): return CheckedCastFromUnsigned<int64_t>(val.u64);

        case CheckedCastOp::Make(CastType::F32, CastType::I8).Raw():  return CheckedCastFromFp<int8_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::U8).Raw():  return CheckedCastFromFp<uint8_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::I16).Raw(): return CheckedCastFromFp<int16_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::U16).Raw(): return CheckedCastFromFp<uint16_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::I32).Raw(): return CheckedCastFromFp<int32_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::U32).Raw(): return CheckedCastFromFp<uint32_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::I64).Raw(): return CheckedCastFromFp<int64_t>(val.f32);
        case CheckedCastOp::Make(CastType::F32, CastType::U64).Raw(): return CheckedCastFromFp<uint64_t>(val.f32);

        case CheckedCastOp::Make(CastType::F64, CastType::I8).Raw():  return CheckedCastFromFp<int8_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::U8).Raw():  return CheckedCastFromFp<uint8_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::I16).Raw(): return CheckedCastFromFp<int16_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::U16).Raw(): return CheckedCastFromFp<uint16_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::I32).Raw(): return CheckedCastFromFp<int32_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::U32).Raw(): return CheckedCastFromFp<uint32_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::I64).Raw(): return CheckedCastFromFp<int64_t>(val.f64);
        case CheckedCastOp::Make(CastType::F64, CastType::U64).Raw(): return CheckedCastFromFp<uint64_t>(val.f64);
    }
    FATAL("invalid cast op: %u", op.Raw());
}

} // namespace Interpretation
