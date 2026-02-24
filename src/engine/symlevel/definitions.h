#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include "string.h"
#include "type_kind.h"
#include <vector>

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
    static TypeDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);
    static TypeDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset);
    static TypeDefinition Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier);

    inline const Offset<String> Name() const { return name; }

private:
    TypeDefinition(IO::FileId fileId, Offset<String> name, uint32_t methodsCount, uint32_t methodsOffset)
        : fileId(fileId),
          name(name),
          methodsCount(methodsCount),
          methodsOffset(methodsOffset)
    {}

    IO::FileId fileId;
    Offset<String> name;
    uint32_t methodsCount;
    uint32_t methodsOffset;
};

class FieldDefinition {
public:
    static FieldDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const Offset<String> Name() const { return name; }

private:
    FieldDefinition(IO::FileId fileId, Offset<String> name, uint32_t idx, uint32_t declIdx, uint32_t typeIdx)
        : fileId(fileId),
          name(name),
          idx(idx),
          declIdx(declIdx),
          typeIdx(typeIdx)
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t idx;
    const uint32_t declIdx;
    const uint32_t typeIdx;
};

class MethodDefinition {
public:
    static MethodDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);
    static MethodDefinition Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset);
    static MethodDefinition Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier);

    inline const Offset<String> Name() const { return name; }

    inline uint32_t GetSigIdx() const
    {
        ASSERTION(false, "implement terms"); // FIXME
        return sigIdx;
    }

    inline Offset<Code> GetCodeOffs() const { return codeOffs; }

    inline IO::FileId FileId() const { return fileId; }

private:
    MethodDefinition(IO::FileId fileId, Offset<String> name, uint32_t sigIdx, Offset<Code> codeOffs)
        : fileId(fileId),
          name(name),
          sigIdx(sigIdx),
          codeOffs(codeOffs)
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t sigIdx;
    const Offset<Code> codeOffs;
};

} // namespace Symlevel
