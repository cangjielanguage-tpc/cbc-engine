#pragma once

#include "access_kind.h"
#include "engine/symlevel/terms.h"
#include "runtimesupport/runtime.h"
#include "runtimesupport/runtime_impl.h"
#include <cstdint>
#include <string>
#include <optional>
#include <string_view>
#include <vector>

namespace API {

class Type;

struct TypeKind;
struct TypeFlag;
struct TypeFlags;

using TypeInfo = RTSupport::TypeInfo<RTSupport::Impl>;

/**
 * @class Type
 * @brief Root of hierarchy of all concrete types.
 *
 * @see Term
 */
class Type {
    using Term = Symlevel::Terms::Term;

public:
    /**
     * @brief The closed term representation of the type.
     */
    virtual Term* AsTerm() = 0;

    /**
     * @brief Runtime information about type. Absent if type is generic.
     */
    virtual std::optional<TypeInfo> GetTypeInfo() = 0;

    /**
     * @brief The size of a field of the given type.
     */
    virtual int FieldSize() = 0;

    /**
     * @brief Offsets to reference fields of the type.
     */
    virtual std::vector<int> RefOffsets() = 0;

    /**
     * @brief Flags of the type.
     */
    virtual TypeFlags Flags() = 0;

    /**
     * @brief Full name of the type.
     */
    virtual std::string_view FullName() = 0;

protected:
    virtual ~Type() = default;
};

struct TypeKind {
public:
    enum Value : uint8_t {
        INVALID,
        CLASS,
        ARRAY,
        INTERFACE,
        RECORD,
        PRIMITIVE
    };

    static constexpr int BIT_COUNT = 3;

    constexpr TypeKind(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr std::string_view const ToString()
    {
        switch (value) {
            case INVALID:   return "INVALID";
            case CLASS:     return "CLASS";
            case ARRAY:     return "ARRAY";
            case INTERFACE: return "INTERFACE";
            case RECORD:    return "RECORD";
            case PRIMITIVE: return "PRIMITIVE";
            default:        return "<invalid/unknown>";
        }
    }

private:
    Value value;
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

} // namespace API
