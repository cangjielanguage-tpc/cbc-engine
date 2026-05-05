#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/offset_sequence.h"
#include "io/file_id.h"
#include "member_index.h"
#include "offset.h"
#include "string.h"
#include "term.h"

namespace Symlevel {

class DefinitionsManager {
public:
    static DefinitionsManager& Of(Engine::Engine& engine);
    DefinitionsManager();
    DefinitionsManager(DefinitionsManager&& manager);
    ~DefinitionsManager();

private:
    class Impl;
    friend class Impl;

    std::unique_ptr<Impl> impl;
};

class TypeDefinition {
public:
    static TypeDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset);
    static TypeDefinition Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }

    inline Engine::Identifier<TypeDefinition> GetIdentifier() const { return identifier; }

    inline const MethodIndex& GetMethodIndex() const { return methods; }

    inline const FieldIndex& GetFieldIndex() const { return fields; }

    inline const OffsetSequence<MethodDefinition>& GetVirtualMethods() const { return virtualMethods; }

private:
    TypeDefinition(
        Engine::Identifier<TypeDefinition> identifier,
        Offset<String> nameOffset,
        MethodIndex methods,
        FieldIndex fields,
        OffsetSequence<MethodDefinition> virtualMethods
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          methods(std::move(methods)),
          fields(std::move(fields)),
          virtualMethods(virtualMethods)
    {}

    Engine::Identifier<TypeDefinition> identifier;
    Offset<String> nameOffset;
    MethodIndex methods;
    FieldIndex fields;
    OffsetSequence<MethodDefinition> virtualMethods;
};

class FieldDefinition {
public:
    static FieldDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);
    static FieldDefinition Resolve(Engine::Session& session, Engine::Identifier<FieldDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }
    inline Engine::IndexIdentifier<Term> FieldType() const { return fieldType; }
    inline Engine::Identifier<FieldDefinition> Identifier() { return identifier; }
    inline FieldFlags Flags() { return flags; }

private:
    FieldDefinition(
        Engine::Identifier<FieldDefinition> identifier,
        Offset<String> nameOffset,
        Engine::IndexIdentifier<Term> refType,
        Engine::IndexIdentifier<Term> fieldType,
        FieldFlags flags,
        std::vector<uint64_t> constValue 
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          refType(refType),
          fieldType(fieldType),
          flags(flags),
          constValue(constValue)
    {}

    Engine::Identifier<FieldDefinition> identifier;
    Offset<String> nameOffset;
    Engine::IndexIdentifier<Term> refType;
    Engine::IndexIdentifier<Term> fieldType;
    FieldFlags flags;
    std::vector<uint64_t> constValue;

};

class MethodDefinition {
public:
    static MethodDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);
    static MethodDefinition Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }

    inline Offset<Code> GetCodeOffset() const { return *codeOffs; }

    inline IO::FileId FileId() const { return identifier.GetFileId(); }

    Engine::Identifier<MethodDefinition> GetIdentifier() const { return identifier; }

private:
    MethodDefinition(
        Engine::Identifier<MethodDefinition> identifier,
        Offset<String> nameOffset,
        std::optional<Offset<Code>> codeOffs
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          codeOffs(codeOffs)
    {}

    Engine::Identifier<MethodDefinition> identifier;
    Offset<String> nameOffset;    std::optional<Offset<Code>> codeOffs;
};

} // namespace Symlevel
