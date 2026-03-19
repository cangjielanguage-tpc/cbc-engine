#pragma once

#include "term.h"
#include "type.h"
#include <cstdint>
#include <optional>
#include <string>

namespace API {

class InstanceField;
class StaticField;

struct FieldFlag;
struct FieldFlags;

/**
 * @class InstanceField
 * @brief Instance field representation of a type.
 *
 * @see Type
 * @see Term
 */
class InstanceField {
public:
    /**
     * @brief The term representation of the field.
     */
    virtual Term* FieldTerm() = 0;

    /**
     * @brief The term representation of the ref type.
     */
    virtual Term* RefTypeTerm() = 0;

    /**
     * @brief The index of the field in total field numbering.
     *
     * Example:
     * @code
     *  open class Foo {
     *      var x: Int32; // ordinal 0
     *      var y: Int32; // ordinal 1
     *  }
     *
     *  open class Bar <: Foo {
     *      var z: Int32; // ordinal 2
     *  }
     *
     *  open class Bax <: Foo {
     *      var w: Int32; // ordinal 3
     *  }
     * @endcode
     */
    virtual int Ordinal() = 0;

    /**
     * @brief The ref type.
     */
    virtual std::optional<Type*> RefType() = 0;

    /**
     * @brief Offset of the field.
     */
    virtual std::optional<int> Offset() = 0;

    /**
     * @brief Flags of the field.
     */
    virtual FieldFlags Flags() = 0;

    /**
     * @brief Full name of the field.
     */
    virtual std::string_view FullName() = 0;

protected:
    virtual ~InstanceField() = default;
};

/**
 * @class StaticField
 * @brief Static/global field representation.
 *
 * @note Static field is always concrete.
 *
 * @see Type
 * @see Term
 */
class StaticField {
public:
    /**
     * @brief The term representation of the field.
     */
    virtual Term* FieldTerm() = 0;

    /**
     * @brief Static field location.
     */
    virtual std::uintptr_t Location() = 0;

    /**
     * @brief Flags of the field.
     */
    virtual FieldFlags Flags() = 0;

    /**
     * @brief Full name of the field.
     */
    virtual std::string_view FullName() = 0;

protected:
    virtual ~StaticField() = default;
};

struct FieldFlag {
public:
    enum Value : uint32_t {
        FINAL,
        STATIC,
        VOLATILE
    };

    static constexpr Value values[] = { FINAL, STATIC, VOLATILE };

    constexpr FieldFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case FINAL:    return "FINAL";
            case STATIC:   return "STATIC";
            case VOLATILE: return "VOLATILE";

            default: return "<invalid/unknown>";
        }
    }

private:
    Value value;
};

struct FieldFlags {
public:
    constexpr FieldFlags() : accessRaw(0), flagsRaw(0) {}

    constexpr AccessKind GetAccessKind() const { return static_cast<AccessKind::Value>(accessRaw); }

    constexpr bool Is(AccessKind kind) const { return GetAccessKind() == kind; }

    constexpr bool Is(FieldFlag flag) const { return flagsRaw & (1 << static_cast<FieldFlag::Value>(flag)); }

    constexpr FieldFlags Or(FieldFlag flag) const
    {
        FieldFlags copy  = *this;
        copy.flagsRaw   |= 1 << flag;
        return copy;
    }

    constexpr FieldFlags Or(FieldFlag flag, bool shouldAdd) const { return shouldAdd ? Or(flag) : *this; }

    constexpr FieldFlags With(AccessKind kind) const
    {
        FieldFlags copy = *this;
        copy.accessRaw  = kind;
        return copy;
    }

    std::string ToString() const
    {
        std::string result;
        result.reserve(32);

        result += GetAccessKind().ToString();

        for (FieldFlag flag : FieldFlag::values) {
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
