#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/sequence.h"
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

    Engine::Identifier<TypeDefinition> const GetIdentifier() { return identifier; }

    Engine::Identifier<String> const GetName() const { return name; }

    MethodIndex const GetMethods() const { return methods; }

    FieldIndex const GetFields() const { return fields; }

    OffsetSequence<MethodDefinition> const GetVirtualMethods() const { return virtualMethods; }

    Engine::RefIdentifier<Term> const GetSuperType() const { return superType; }

    TypeFlags const GetFlags() const { return flags; }

    RefSequence<Term> GetInterfaces() { return interfaces; }

private:
    TypeDefinition(
        Engine::Identifier<TypeDefinition> const identifier,
        Engine::Identifier<String> const name,
        MethodIndex const methods,
        FieldIndex const fields,
        OffsetSequence<MethodDefinition> const virtualMethods,
        Engine::RefIdentifier<Term> const superType,
        TypeFlags flags
    )
        : identifier(identifier),
          name(name),
          methods(methods),
          fields(fields),
          virtualMethods(virtualMethods),
          superType(superType),
          flags(flags)
    {}

    Engine::Identifier<TypeDefinition> identifier;
    Engine::Identifier<String> name;
    MethodIndex methods;
    FieldIndex fields;
    OffsetSequence<MethodDefinition> virtualMethods;
    Engine::RefIdentifier<Term> superType;
    TypeFlags flags;

    RefSequence<Term> interfaces{};
};

class FieldDefinition {
public:
    static FieldDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);
    static FieldDefinition Resolve(Engine::Session& session, Engine::Identifier<FieldDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }

    inline Engine::RefIdentifier<Term> FieldType() const { return fieldType; }

    inline Engine::Identifier<FieldDefinition> Identifier() { return identifier; }

    inline FieldFlags Flags() const { return flags; }

private:
    FieldDefinition(
        Engine::Identifier<FieldDefinition> identifier,
        Offset<String> nameOffset,
        Engine::RefIdentifier<Term> fieldType,
        FieldFlags flags,
        std::vector<uint64_t> constValue
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          fieldType(fieldType),
          flags(flags),
          constValue(constValue)
    {}

    Engine::Identifier<FieldDefinition> identifier;
    Offset<String> nameOffset;
    Engine::RefIdentifier<Term> fieldType;
    FieldFlags flags;
    std::vector<uint64_t> constValue;
};

class MethodDefinition {
public:
    static MethodDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);
    static MethodDefinition Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);

    inline Engine::Identifier<String> Name() const { return Engine::Identifier(nameOffset, identifier.GetFileId()); }

    inline Engine::RefIdentifier<Term> Signature() const { return signature; }

    inline std::optional<Engine::Identifier<Code>> MethodCode() const { return code; }

    std::optional<Engine::Identifier<String>> SourceFile() const { return sourceFile; }

    std::optional<Engine::Identifier<String>> SourceFullName() const { return sourceFullName; }

    std::optional<Engine::Identifier<String>> LinkageName() const { return linkageName; }

    inline IO::FileId FileId() const { return identifier.GetFileId(); }

    Engine::Identifier<MethodDefinition> GetIdentifier() const { return identifier; }

    MethodFlags GetFlags() const { return flags; }

private:
    MethodDefinition(
        Engine::Identifier<MethodDefinition> identifier,
        Offset<String> nameOffset,
        Engine::RefIdentifier<Term> signature,
        MethodFlags flags
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          signature(signature),
          flags(flags)
    {}

    Engine::Identifier<MethodDefinition> identifier;
    Engine::RefIdentifier<Term> signature;
    Offset<String> nameOffset;
    MethodFlags flags;

    std::optional<Engine::Identifier<Code>> code             = std::nullopt;
    std::optional<Engine::Identifier<String>> sourceFile     = std::nullopt;
    std::optional<Engine::Identifier<String>> sourceFullName = std::nullopt;
    std::optional<Engine::Identifier<String>> linkageName    = std::nullopt;
};

} // namespace Symlevel
