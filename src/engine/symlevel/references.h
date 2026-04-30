#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "io/file_id.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

class MethodReference {
public:
    static MethodReference Parse(
        Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset
    );

    inline const auto Name() const { return Engine::Identifier(name, fileId); }

    inline const auto RefType() const { return Engine::IndexIdentifier(refType, fileId); }

    inline const auto MethodSig() const { return Engine::IndexIdentifier(methodSig, fileId); }

private:
    MethodReference(IO::FileId fileId, Offset<String> name, Index<Term> refType, Index<Term> methodSig)
        : fileId(fileId),
          name(name),
          refType(refType),
          methodSig(methodSig)
    {}

    IO::FileId fileId;
    Offset<String> name;
    Index<Term> refType;
    Index<Term> methodSig;
};

class FieldReference {
public:
    static FieldReference Parse(
        Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset
    );

    inline const auto Name() const { return Engine::Identifier(name, fileId); }

    inline const auto RefType() const { return Engine::IndexIdentifier(refType, fileId); }

    inline const auto MethodSig() const { return Engine::IndexIdentifier(fieldType, fileId); }

    inline const bool IsRecord() const { return isRecord; }

private:
    FieldReference(IO::FileId fileId, Offset<String> name, Index<Term> refType, Index<Term> fieldType, bool isRecord)
        : fileId(fileId),
          name(name),
          refType(refType),
          fieldType(fieldType),
          isRecord(isRecord)
    {}

    IO::FileId fileId;
    Offset<String> name;
    Index<Term> refType;
    Index<Term> fieldType;
    bool isRecord;
};

} // namespace Symlevel
