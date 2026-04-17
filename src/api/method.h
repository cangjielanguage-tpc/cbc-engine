#pragma once

#include "interpreter/function_handle.h"
#include "type.h"
#include <optional>
#include <string>

namespace API {

class DirectMethod;
class VirtualMethod;
class InterfaceMethod;

struct MethodFlag;
struct MethodFlags;

/**
 * @class Method
 * @brief Base class for method representation.
 *
 * @see DirectMethod
 * @see VirtualMethod
 * @see InterfaceMethod
 */
class Method {
public:
    /**
     * @brief The signature used for actual ABI of a method invocation.
     */
    virtual Symlevel::Term* ABISignature() const = 0;

    /**
     * @brief The ref type.
     */
    virtual Type* RefType() const = 0;

    virtual MethodFlags Flags() const = 0;

    virtual Symlevel::String Name() const = 0;

protected:
    virtual ~Method() = default;
};

/**
 * @class DirectMethod
 * @brief Direct method representation.
 *
 * @see Term
 * @see Type
 */
class DirectMethod : public Method {
public:
    virtual Interpretation::FunctionHandle* FUH() const = 0;

    virtual AotCodeAddr TargetAddr() const = 0;

protected:
    virtual ~DirectMethod() = default;
};

/**
 * @class VirtualMethod
 * @brief Virtual method representation.
 *
 * @see Term
 * @see Type
 */
class VirtualMethod : public Method {
public:
    virtual uint16_t VNum() const = 0;

    virtual uint16_t ExtDefNum() const = 0;

protected:
    virtual ~VirtualMethod() = default;
};

/**
 * @class InterfaceMethod
 * @brief Interface method representation.
 *
 * @see Term
 * @see Type
 */
class InterfaceMethod : public Method {
public:
    virtual uint16_t INum() const = 0;

protected:
    virtual ~InterfaceMethod() = default;
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

} // namespace API
