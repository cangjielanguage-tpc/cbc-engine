#pragma once

#include "interpreter/function_handle.h"
#include "term.h"
#include "type.h"
#include <optional>
#include <string>

namespace API {

class Method;

struct MethodFlag;
struct MethodFlags;

/**
 * @class Method
 * @brief Method representation.
 *
 * @see Term
 * @see Type
 */
class Method {
public:
    /**
     * @brief The signature used for actual ABI of a method invocation.
     */
    virtual Term* ABISignature() = 0;

    /**
     * @brief The ref type.
     */
    virtual std::optional<Type*> RefType() = 0;

    virtual std::optional<Interpretation::FunctionHandle*> FUH() = 0;

    /**
     * @brief The flags of the method.
     */
    virtual MethodFlags Flags() = 0;

    /**
     * @brief Full name of the field.
     */
    virtual std::string_view FullName() = 0;

protected:
    virtual ~Method() = default;
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

    inline constexpr AccessKind GetAccessKind() const { return static_cast<AccessKind::Value>(accessRaw); }

    inline constexpr bool Is(AccessKind kind) const { return GetAccessKind() == kind; }

    inline constexpr bool Is(MethodFlag flag) const { return flagsRaw & (1 << static_cast<MethodFlag::Value>(flag)); }

    inline constexpr MethodFlags Or(MethodFlag flag) const
    {
        MethodFlags copy  = *this;
        copy.flagsRaw    |= 1 << flag;
        return copy;
    }

    inline constexpr MethodFlags Or(MethodFlag flag, bool shouldAdd) const { return shouldAdd ? Or(flag) : *this; }

    inline constexpr MethodFlags With(AccessKind kind) const
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

} // namespace API
