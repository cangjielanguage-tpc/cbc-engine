#pragma once

#include "engine/symlevel/terms.h"
#include "type.h"
#include <cstdint>
#include <optional>
#include <string>
#include <utils/assertion.h>

namespace API {

class InstanceField;
class StaticField;

struct FieldFlag;
struct FieldFlags;

/**
 * @class Field
 * @brief Base class for field representation.
 *
 * @see InstanceField
 * @see StaticField
 */
class Field {
    using Term = Symlevel::Terms::Term;

public:
    /**
     * @brief Type of field.
     */
    virtual Type* FieldType() = 0;

    /**
     * @brief The ref type.
     */
    virtual Type* RefType() = 0;

    /**
     * @brief Full name of the field.
     */
    virtual Symlevel::String Name() = 0;

    /**
     * @brief Flags of the field.
     */
    virtual FieldFlags Flags() = 0;

protected:
    virtual ~Field() = default;
};

/**
 * @class InstanceField
 * @brief Instance field representation of a type.
 *
 * @see Type
 * @see Term
 */
class InstanceField : public Field {
    using Term = Symlevel::Terms::Term;

public:
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
     * @brief Offset of the field.
     */
    virtual uint32_t Offset() = 0;

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
class StaticField : public Field {
public:
    /**
     * @brief Static field location.
     */
    virtual std::uintptr_t Location() = 0;

protected:
    virtual ~StaticField() = default;
};

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
            case FINAL:    return "FINAL";
            case STATIC:   return "STATIC";
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

} // namespace API
