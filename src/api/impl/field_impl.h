#pragma once

#include "api/field.h"
#include "api/type.h"
#include "engine/symlevel/terms.h"

namespace API {

class InstanceFieldImpl : public InstanceField {
    using Term = Symlevel::Term;

public:
    InstanceFieldImpl(Symlevel::String name, int ordinal, FieldFlags flags, Type* fieldType, Type* refType);

    Type* FieldType() override { return fieldType; }

    Type* RefType() override { return refType; }

    Symlevel::String Name() override { return name; }

    FieldFlags Flags() override { return flags; }

    int Ordinal() override { return ordinal; }

    uint32_t Offset() override;

private:
    Symlevel::String name;
    Type* fieldType;
    Type* refType;
    int ordinal;
    std::optional<uint32_t> offset;
    FieldFlags flags;
};

class StaticFieldImpl : public StaticField {
    using Term = Symlevel::Term;

public:
    StaticFieldImpl(uintptr_t location, Symlevel::String name, FieldFlags flags, Type* fieldType, Type* refType);

    Type* FieldType() override { return fieldType; }

    Type* RefType() override { return refType; }

    Symlevel::String Name() override { return name; }

    FieldFlags Flags() override { return flags; }

    std::uintptr_t Location() override {
        ASSERTION(location != 0, "static field ref points to incorrect location"); 
        return location; 
    }

private:
    Symlevel::String name;
    Type* fieldType;
    Type* refType;
    uintptr_t location;
    FieldFlags flags;
};

} // namespace API
