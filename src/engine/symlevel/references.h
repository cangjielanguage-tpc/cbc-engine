#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

class MethodReference;
class FieldReference;

class MethodReference {
public:
    static MethodReference Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset);

    inline const String Name() const { return name; }

    inline const Term RefType() const { return refType; }

    inline const Term MethodSig() const { return methodSig; }

private:
    MethodReference(String name, Term refType, Term methodSig) : name(name), refType(refType), methodSig(methodSig) {}

    const String name;
    const Term refType;
    const Term methodSig;
};

class FieldReference {
public:
    static FieldReference Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset);

    inline const String Name() const { return name; }

    inline const Term RefType() const { return refType; }

    inline const Term FieldType() const { return fieldType; }

private:
    FieldReference(String name, Term refType, Term fieldType) : name(name), refType(refType), fieldType(fieldType) {}

    const String name;
    const Term refType;
    const Term fieldType;
};

} // namespace Symlevel
