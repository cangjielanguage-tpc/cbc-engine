#pragma once

#include <vector>

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "member_index.h"
#include "offset.h"
#include "string.h"
#include "type_kind.h"

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

private:
    TypeDefinition(
        Engine::Identifier<TypeDefinition> identifier, Offset<String> nameOffset, MethodIndex methods, FieldIndex fields
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          methods(std::move(methods)),
          fields(std::move(fields))
    {}

    Engine::Identifier<TypeDefinition> identifier;
    Offset<String> nameOffset;
    MethodIndex methods;
    FieldIndex fields;
};

class FieldDefinition {
public:
    static FieldDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);
    static FieldDefinition Resolve(Engine::Session& session, Engine::Identifier<FieldDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }

private:
    FieldDefinition(
        Engine::Identifier<FieldDefinition> identifier,
        Offset<String> nameOffset,
        uint32_t idx,
        uint32_t declIdx,
        uint32_t typeIdx
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          idx(idx),
          declIdx(declIdx),
          typeIdx(typeIdx)
    {}

    Engine::Identifier<FieldDefinition> identifier;
    Offset<String> nameOffset;
    uint32_t idx;
    uint32_t declIdx;
    uint32_t typeIdx;
};

class MethodDefinition {
public:
    static MethodDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);
    static MethodDefinition Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier);
    static String ParseName(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);

    inline Offset<String> NameOffset() const { return nameOffset; }

    inline uint32_t GetSigIdx() const
    {
        FATAL("implement terms"); // FIXME
        return sigIdx;
    }

    inline Offset<Code> GetCodeOffset() const { return *codeOffs; }

    inline IO::FileId FileId() const { return identifier.GetFileId(); }

    Engine::Identifier<MethodDefinition> GetIdentifier() const { return identifier; }

private:
    MethodDefinition(
        Engine::Identifier<MethodDefinition> identifier,
        Offset<String> nameOffset,
        uint32_t sigIdx,
        std::optional<Offset<Code>> codeOffs
    )
        : identifier(identifier),
          nameOffset(nameOffset),
          sigIdx(sigIdx),
          codeOffs(codeOffs)
    {}

    Engine::Identifier<MethodDefinition> identifier;
    Offset<String> nameOffset;
    uint32_t sigIdx;
    std::optional<Offset<Code>> codeOffs;
};

} // namespace Symlevel
