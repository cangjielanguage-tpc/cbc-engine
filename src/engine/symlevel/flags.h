#pragma once

#include "access_kind.h"
#include "type_kind.h"
#include "utils/ostream.h"

#include <stdint.h>
#include <string>
#include <string_view>

namespace Symlevel {

#define TYPE_FLAGS(X)                                                                                                  \
    X(FINAL)                                                                                                           \
    X(ABSTRACT)                                                                                                        \
    X(SEALED)                                                                                                          \
    X(AOT)

#define FIELD_FLAGS(X)                                                                                                 \
    X(STATIC)                                                                                                          \
    X(AOT)                                                                                                             \
    X(FINAL)

#define METHOD_FLAGS(X)                                                                                                \
    X(FINAL)                                                                                                           \
    X(STATIC)                                                                                                          \
    X(VIRTUAL)                                                                                                         \
    X(ABSTRACT)                                                                                                        \
    X(FOREIGN)                                                                                                         \
    X(MUT)                                                                                                             \
    X(AOT)

#define FLAG_LIST(flag) flag,
#define FLAG_C_STR(flag)                                                                                               \
    case flag: return #flag;

struct FieldFlag {
public:
    enum Value : uint8_t {
        FIELD_FLAGS(FLAG_LIST)
    };

    static constexpr Value values[] = { FIELD_FLAGS(FLAG_LIST) };

    constexpr FieldFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr char const* CStr() const
    {
        switch (value) {
            FIELD_FLAGS(FLAG_C_STR)
        }
        return "<invalid>";
    }

    constexpr std::string_view ToString() const { return std::string_view(CStr()); }

private:
    Value value;
};

struct MethodFlag {
public:
    enum Value : uint32_t {
        METHOD_FLAGS(FLAG_LIST)
    };

    static constexpr Value values[] = { METHOD_FLAGS(FLAG_LIST) };

    constexpr MethodFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr char const* CStr() const
    {
        switch (value) {
            METHOD_FLAGS(FLAG_C_STR)
        }
        return "<invalid>";
    }

    constexpr std::string_view ToString() const { return std::string_view(CStr()); }

private:
    Value value;
};

struct TypeFlag {
public:
    enum Value : uint32_t {
        TYPE_FLAGS(FLAG_LIST)
    };

    static constexpr Value values[] = { TYPE_FLAGS(FLAG_LIST) };

    constexpr TypeFlag(const Value value) : value(value) {}

    constexpr operator Value() const { return value; }

    constexpr char const* CStr() const
    {
        switch (value) {
            TYPE_FLAGS(FLAG_C_STR)
        }
        return "<invalid>";
    }

    constexpr std::string_view ToString() const { return std::string_view(CStr()); }

private:
    Value value;
};

#undef TYPE_FLAGS
#undef FIELD_FLAGS
#undef METHOD_FLAGS
#undef FLAG_C_STR
#undef FLAG_LIST

struct FieldFlags {
public:
    constexpr FieldFlags() : flagsRaw(0), accessRaw(0) {}

    constexpr AccessKind GetAccessKind() const { return static_cast<AccessKind::Value>(accessRaw); }

    constexpr bool Is(AccessKind kind) const { return GetAccessKind() == kind; }

    constexpr bool Is(FieldFlag flag) const { return flagsRaw & (1 << flag); }

    constexpr bool IsNot(FieldFlag flag) const { return !Is(flag); }

    constexpr FieldFlags With(AccessKind accessKind) const
    {
        FieldFlags copy = *this;
        copy.accessRaw   = accessRaw;
        return copy;
    }

    constexpr FieldFlags Or(FieldFlag flag) const
    {
        FieldFlags copy  = *this;
        copy.flagsRaw   |= 1 << flag;
        return copy;
    }

    std::string ToString() const;

private:
    uint16_t flagsRaw : 16 - AccessKind::BIT_COUNT;
    uint16_t accessRaw : AccessKind::BIT_COUNT;
};

struct MethodFlags {
public:
    constexpr MethodFlags() : accessRaw(AccessKind::INVALID), flagsRaw(0) {}

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

    std::string ToString() const;

private:
    uint32_t accessRaw : AccessKind::BIT_COUNT;
    uint32_t flagsRaw : 30;

    static_assert(AccessKind::BIT_COUNT + 30 == sizeof(uint32_t) * 8);
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

    std::string ToString() const;

private:
    uint32_t accessRaw : AccessKind::BIT_COUNT;
    uint32_t kindRaw : TypeKind::BIT_COUNT;
    uint32_t flagsRaw : 27;

    static_assert(AccessKind::BIT_COUNT + TypeKind::BIT_COUNT + 27 == 32);
};

Stream::Output& operator<<(Stream::Output& stream, TypeFlags flags);
Stream::Output& operator<<(Stream::Output& stream, MethodFlags flags);
Stream::Output& operator<<(Stream::Output& stream, FieldFlags flags);
Stream::Output& operator<<(Stream::Output& stream, TypeFlag flag);
Stream::Output& operator<<(Stream::Output& stream, MethodFlag flag);
Stream::Output& operator<<(Stream::Output& stream, FieldFlag flag);

} // namespace Symlevel
