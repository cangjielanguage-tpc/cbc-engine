#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "cbc/isa.h"
#include "ectype.h"

namespace Interpretation {

using ConvertType = Cbc::Format::ConvertType;

Value::Primitive UnsupportedCastFrom(ConvertType ct)
{
    ASSERTION(false, "unsupported cast: from %s)", ct.ToStr());
    return Value::Primitive { .u64 = 0 };
}

Value::Primitive UnsupportedCastTo(ConvertType ct)
{
    ASSERTION(false, "unsupported cast: to %s)", ct.ToStr());
    return Value::Primitive { .u64 = 0 };
}

template <typename T> Value::Primitive MakeIntPrim(T value)
{
    static_assert(std::is_integral_v<T>, "integral value is expected");
    return Value::Primitive { .u64 = static_cast<uint64_t>(value) };
}

template <typename T> Value::Primitive MakeFpPrim(T value)
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

template <typename To, typename From> Value::Primitive CastInt(From value)
{
    using Narrow = std::conditional_t<(sizeof(From) < sizeof(To)), From, To>;
    return MakeIntPrim(static_cast<Narrow>(value));
}

template <typename To, typename From> Value::Primitive CastFpToInt(From value)
{
    if (std::isnan(value)) {
        return MakeIntPrim(static_cast<To>(0));
    }
    if (value >= static_cast<From>(std::numeric_limits<To>::max())) {
        return MakeIntPrim(std::numeric_limits<To>::max());
    }
    if constexpr (std::is_signed_v<To>) {
        if (value <= static_cast<From>(std::numeric_limits<To>::min())) {
            return MakeIntPrim(std::numeric_limits<To>::min());
        }
    }
    return MakeIntPrim(static_cast<To>(value));
}

template <typename From> Value::Primitive CastFrom32(ConvertType toType, From value)
{
    switch (toType) {
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

Value::Primitive CastFromI64(ConvertType toType, int64_t value)
{
    switch (toType) {
        case ConvertType::I32: return CastInt<int32_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

Value::Primitive CastFromU64(ConvertType toType, uint64_t value)
{
    switch (toType) {
        case ConvertType::I32: return CastInt<int32_t>(value);
        case ConvertType::U32: return CastInt<uint32_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

Value::Primitive CastFromF32(ConvertType toType, float value)
{
    switch (toType) {
        case ConvertType::I32: return CastFpToInt<int32_t>(value);
        case ConvertType::U32: return CastFpToInt<uint32_t>(value);
        case ConvertType::I64: return CastFpToInt<int64_t>(value);
        case ConvertType::U64: return CastFpToInt<uint64_t>(value);
        case ConvertType::F64: return MakeFpPrim(static_cast<double>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

Value::Primitive CastFromF64(ConvertType toType, double value)
{
    switch (toType) {
        case ConvertType::I32: return CastFpToInt<int32_t>(value);
        case ConvertType::U32: return CastFpToInt<uint32_t>(value);
        case ConvertType::I64: return CastFpToInt<int64_t>(value);
        case ConvertType::U64: return CastFpToInt<uint64_t>(value);
        case ConvertType::F32: return MakeFpPrim(static_cast<float>(value));
        default:               return UnsupportedCastTo(toType);
    }
}

Value::Primitive CastPrim(ConvertType toType, ConvertType fromType, Value::Primitive val)
{
    switch (fromType) {
        case ConvertType::I32: return CastFrom32(toType, static_cast<int32_t>(val.u32));
        case ConvertType::U32: return CastFrom32(toType, val.u32);
        case ConvertType::I64: return CastFromI64(toType, static_cast<int64_t>(val.u64));
        case ConvertType::U64: return CastFromU64(toType, val.u64);

        case ConvertType::F32: return CastFromF32(toType, val.f32);
        case ConvertType::F64: return CastFromF64(toType, val.f64);

        // TODO: support fp16 arithmetics and casts
        default: return UnsupportedCastFrom(fromType);
    }
}

} // namespace Interpretation
