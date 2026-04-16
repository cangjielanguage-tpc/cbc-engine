#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "string.h"
#include "terms.h"

namespace Symlevel {

class MethodReference;
class FieldReference;

class MethodReference {
public:
    static std::optional<MethodReference> ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset
    );

    inline const IO::FileId FileId() const { return fileId; }

    inline const String Name() const { return name; }

    inline const Terms::Term RefType() const { return refType; }

    inline const Terms::Term MethodSig() const { return methodSig; }

private:
    MethodReference(IO::FileId fileId, String name, Terms::Term refType, Terms::Term methodSig)
        : fileId(fileId),
          name(name),
          refType(refType),
          methodSig(methodSig)
    {}

    IO::FileId fileId;
    String name;
    Terms::Term refType;
    Terms::Term methodSig;
};

class FieldReference {
public:
    static std::optional<FieldReference> ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset
    );

    inline const String Name() const { return name; }

    inline const Terms::Term RefType() const { return refType; }

    inline const Terms::Term FieldType() const { return fieldType; }

private:
    FieldReference(String name, Terms::Term refType, Terms::Term fieldType)
        : name(name),
          refType(refType),
          fieldType(fieldType)
    {}

    String name;
    Terms::Term refType;
    Terms::Term fieldType;
};

} // namespace Symlevel
