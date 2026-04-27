#pragma once

#include "api/field.h"
#include "api/type.h"
#include "engine/symlevel/terms.h"

namespace API {

class InstanceFieldImpl : public InstanceField {
public:
    InstanceFieldImpl(Symlevel::String name, int ordinal, FieldFlags flags, Type* fieldType, Type* refType);

    std::optional<Type*> FieldType() override { return fieldType; }

    std::optional<Type*> RefType() override { return refType; }

    Symlevel::String Name() override { return name; }

    FieldFlags Flags() override { return flags; }

    int Ordinal() override { return ordinal; }

    std::optional<uint32_t> Offset() override;

private:
    Symlevel::String name;
    std::optional<Type*> fieldType;
    std::optional<Type*> refType;
    int ordinal;
    std::optional<uint32_t> offset;
    FieldFlags flags;
};

class StaticFieldImpl : public StaticField {
public:
    StaticFieldImpl(
        uintptr_t location,
        Symlevel::String name,
        FieldFlags flags,
        std::optional<Type*> fieldType = std::nullopt,
        std::optional<Type*> refType   = std::nullopt
    );

    std::optional<Type*> FieldType() override { return fieldType; }

    std::optional<Type*> RefType() override { return refType; }

    Symlevel::String Name() override { return name; }

    FieldFlags Flags() override { return flags; }

    std::uintptr_t Location() override
    {
        ASSERTION(location != 0, "static field ref points to incorrect location");
        return location;
    }

private:
    Symlevel::String name;
    std::optional<Type*> fieldType;
    std::optional<Type*> refType;
    uintptr_t location;
    FieldFlags flags;
};

} // namespace API
