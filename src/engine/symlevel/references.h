#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "string.h"
#include "terms.h"

namespace Symlevel {

class MethodReference;
class FieldReference;

enum MethodAccessKind : uint16_t;

class MethodReference {
public:
    static std::optional<MethodReference> ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset
    );

    inline const IO::FileId FileId() const { return fileId; }

    inline const String Name() const { return name; }

    inline const Terms::Term RefType() const { return refType; }

    inline const Terms::Term MethodSig() const { return methodSig; }

    inline const MethodAccessKind AccessKind() const { return accessKind; }

private:
    MethodReference(
        IO::FileId fileId, String name, Terms::Term refType, Terms::Term methodSig, MethodAccessKind accessKind
    )
        : fileId(fileId),
          name(name),
          refType(refType),
          methodSig(methodSig),
          accessKind(accessKind)
    {}

    IO::FileId fileId;
    String name;
    Terms::Term refType;
    Terms::Term methodSig;
    MethodAccessKind accessKind;
};

class FieldReference {
public:
    static std::optional<FieldReference> ParseAndResolve(
        Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset
    );

    inline const String Name() const { return name; }

    inline const Terms::Term RefType() const { return refType; }

    inline const Terms::Term FieldType() const { return fieldType; }

    inline const bool IsRecord() const { return isRecord; }

private:
    FieldReference(String name, Terms::Term refType, Terms::Term fieldType, bool isRecord)
        : name(name),
          refType(refType),
          fieldType(fieldType),
          isRecord(isRecord)
    {}

    String name;
    Terms::Term refType;
    Terms::Term fieldType;
    bool isRecord;
};

enum MethodAccessKind : uint16_t {
    DIRECT,
    VIRTUAL,
};

} // namespace Symlevel
