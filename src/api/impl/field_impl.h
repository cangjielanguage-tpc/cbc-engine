#pragma once

#include "api/type.h"
#include "api/field.h"
#include "engine/symlevel/terms.h"

namespace API {

class InstanceFieldImpl: public InstanceField {
    using Term = Symlevel::Terms::Term;

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
    std::optional<Type*> fieldType; // TODO should it be optional?
    std::optional<Type*> refType; // TODO should it be optional?
    int ordinal;
    std::optional<uint32_t> offset;
    FieldFlags flags;
};

class StaticFieldImpl: public StaticField {
    using Term = Symlevel::Terms::Term;

public:
    StaticFieldImpl(uintptr_t location, Symlevel::String name, FieldFlags flags, Type* fieldType, Type* refType);

    std::optional<Type*> FieldType() override { return fieldType; }

    std::optional<Type*> RefType() override { return refType; }

    Symlevel::String Name() override { return name; }

    FieldFlags Flags() override { return flags; }

    std::uintptr_t Location() override { return location; }

private:
    Symlevel::String name;
    std::optional<Type*> fieldType; // TODO should it be optional?
    std::optional<Type*> refType; // TODO should it be optional?
    uintptr_t location;
    FieldFlags flags;
};

} // namespace API