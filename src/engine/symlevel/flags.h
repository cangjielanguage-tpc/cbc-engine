#pragma once

#include "access_kind.h"
#include "type_kind.h"

#include <stdint.h>
#include <string>
#include <string_view>

namespace Symlevel {

struct FieldFlag {
public:
    enum Shift : uint8_t {
        STATIC,
        FINAL,
        VOLATILE,
        RECORD,
    };

    static constexpr Shift variants[] = { STATIC, FINAL, VOLATILE, RECORD };

    constexpr FieldFlag(const Shift shift) : shift(shift) {}

    constexpr operator Shift() const { return shift; }

    constexpr std::string_view const ToString()
    {
        switch (shift) {
            case STATIC:   return "STATIC";
            case FINAL:    return "FINAL";
            case VOLATILE: return "VOLATILE";
            case RECORD:   return "RECORD";

            default: return "<invalid/unknown>";
        }
    }

private:
    Shift shift;
};

struct FieldFlags {
public:
    constexpr FieldFlags(uint8_t flags) : flagsRaw(flags) {}

    constexpr FieldFlags() : flagsRaw(0) {}

    constexpr void With(FieldFlag::Shift pos) { flagsRaw |= 1 << static_cast<FieldFlag::Shift>(pos); }

    constexpr FieldFlags With(FieldFlags other) { return flagsRaw | other.flagsRaw; }

    constexpr bool Is(FieldFlag flag) const { return flagsRaw & (1 << static_cast<FieldFlag::Shift>(flag)); }

    constexpr bool IsNot(FieldFlag flag) const { return !Is(flag); }

    constexpr FieldFlags Or(FieldFlag flag) const
    {
        FieldFlags copy  = *this;
        copy.flagsRaw   |= 1 << flag;
        return copy;
    }

    constexpr FieldFlags Or(FieldFlag flag, bool shouldAdd) const { return shouldAdd ? Or(flag) : *this; }

    std::string ToString() const
    {
        std::string result;
        result.reserve(32);

        for (FieldFlag flag : FieldFlag::variants) {
            if (Is(flag)) {
                result += " ";
                result += flag.ToString();
            }
        }

        return result;
    }

private:
    uint16_t flagsRaw : 14;
};

struct MethodFlag {
public:
    enum Value : uint32_t {
        FINAL,
        OPEN, // TODO: this flag must be computable (or vice versa with FINAL)
        STATIC,
        ABSTRACT,
        RTS_PROC,
        C_ANNOTATED,
        FOREIGN,
        MUT,
        REDEF,
        OVERRIDE,
        HAS_MUT_PARAM,
        HAS_UG_DESC_PARAM,
        HAS_THIS_TYPE_INFO_PARAM,
        HAS_RET_BY_VAL_PARAM,
        HAS_C_FUNC_RET_BY_VAL_PARAM,
        HAS_RECEIVER,
    };

    static constexpr Value values[] = {
        FINAL,
        OPEN,
        STATIC,
        ABSTRACT,
        RTS_PROC,
        C_ANNOTATED,
        FOREIGN,
        MUT,
        REDEF,
        OVERRIDE,
        HAS_MUT_PARAM,
        HAS_UG_DESC_PARAM,
        HAS_THIS_TYPE_INFO_PARAM,
        HAS_RET_BY_VAL_PARAM,
        HAS_C_FUNC_RET_BY_VAL_PARAM,
        HAS_RECEIVER,
    };

    constexpr MethodFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case FINAL:                       return "FINAL";
            case OPEN:                        return "OPEN";
            case STATIC:                      return "STATIC";
            case ABSTRACT:                    return "ABSTRACT";
            case RTS_PROC:                    return "RTS_PROC";
            case C_ANNOTATED:                 return "C_ANNOTATED";
            case FOREIGN:                     return "FOREIGN";
            case MUT:                         return "MUT";
            case REDEF:                       return "REDEF";
            case OVERRIDE:                    return "OVERRIDE";
            case HAS_MUT_PARAM:               return "HAS_MUT_PARAM";
            case HAS_UG_DESC_PARAM:           return "HAS_UG_DESC_PARAM";
            case HAS_THIS_TYPE_INFO_PARAM:    return "HAS_THIS_TYPE_INFO_PARAM";
            case HAS_RET_BY_VAL_PARAM:        return "HAS_RET_BY_VAL_PARAM";
            case HAS_C_FUNC_RET_BY_VAL_PARAM: return "HAS_C_FUNC_RET_BY_VAL_PARAM";
            case HAS_RECEIVER:                return "HAS_RECEIVER";

            default: return "<invalid/unknown>";
        }
    }

private:
    Value value;
};

struct MethodFlags {
public:
    constexpr MethodFlags() : accessRaw(0), flagsRaw(0) {}

    constexpr AccessKind GetAccessKind() const { return static_cast<AccessKind::Value>(accessRaw); }

    constexpr bool Is(AccessKind kind) const { return GetAccessKind() == kind; }

    constexpr bool Is(MethodFlag flag) const { return flagsRaw & (1 << static_cast<MethodFlag::Value>(flag)); }

    constexpr MethodFlags Or(MethodFlag flag) const
    {
        MethodFlags copy  = *this;
        copy.flagsRaw    |= 1 << flag;
        return copy;
    }

    constexpr MethodFlags Or(MethodFlag flag, bool shouldAdd) const { return shouldAdd ? Or(flag) : *this; }

    constexpr MethodFlags With(AccessKind kind) const
    {
        MethodFlags copy = *this;
        copy.accessRaw   = kind;
        return copy;
    }

    std::string ToString() const
    {
        std::string result;
        result.reserve(32);

        result += GetAccessKind().ToString();

        for (MethodFlag flag : MethodFlag::values) {
            if (Is(flag)) {
                result += " ";
                result += flag.ToString();
            }
        }

        return result;
    }

private:
    uint32_t accessRaw : AccessKind::BIT_COUNT;
    uint32_t flagsRaw : 30;

    static_assert(AccessKind::BIT_COUNT + 30 == sizeof(uint32_t) * 8);
};

struct TypeFlag {
public:
    enum Value : uint32_t {
        FINAL,
        ABSTRACT,
        SEALED
    };

    static constexpr Value values[] = { FINAL, ABSTRACT, SEALED };

    constexpr TypeFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case FINAL:    return "FINAL";
            case ABSTRACT: return "ABSTRACT";
            case SEALED:   return "SEALED";

            default: return "<invalid/unknown>";
        }
    }

private:
    Value value;
};

struct TypeFlags {
public:
    constexpr TypeFlags() : accessRaw(0), kindRaw(0), flagsRaw(0) {}

    constexpr AccessKind GetAccessKind() const { return static_cast<AccessKind::Value>(accessRaw); }

    constexpr TypeKind GetTypeKind() const { return static_cast<TypeKind::Value>(kindRaw); }

    constexpr bool Is(AccessKind kind) const { return GetAccessKind() == kind; }

    constexpr bool Is(TypeKind kind) const { return GetTypeKind() == kind; }

    constexpr bool Is(TypeFlag flag) const { return flagsRaw & (1u << static_cast<TypeFlag::Value>(flag)); }

    constexpr TypeFlags Or(TypeFlag flag) const
    {
        TypeFlags copy  = *this;
        copy.flagsRaw  |= 1u << flag;
        return copy;
    }

    constexpr TypeFlags Or(TypeFlag flag, bool shouldAdd) const { return shouldAdd ? Or(flag) : *this; }

    constexpr TypeFlags With(AccessKind kind) const
    {
        TypeFlags copy = *this;
        copy.accessRaw = kind;
        return copy;
    }

    constexpr TypeFlags With(TypeKind kind) const
    {
        TypeFlags copy = *this;
        copy.kindRaw   = kind;
        return copy;
    }

    std::string ToString() const
    {
        std::string result;
        result.reserve(32);

        result += GetAccessKind().ToString();
        result += " ";
        result += GetTypeKind().ToString();

        for (TypeFlag flag : TypeFlag::values) {
            if (Is(flag)) {
                result += " ";
                result += flag.ToString();
            }
        }

        return result;
    }

private:
    uint32_t accessRaw : AccessKind::BIT_COUNT;
    uint32_t kindRaw : TypeKind::BIT_COUNT;
    uint32_t flagsRaw : 27;

    static_assert(AccessKind::BIT_COUNT + TypeKind::BIT_COUNT + 27 == sizeof(uint32_t) * 8);
};

} // namespace Symlevel
