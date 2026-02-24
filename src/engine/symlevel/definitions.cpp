#include "definitions.h"

namespace Symlevel {

class DefinitionsManager::Impl {
    // TODO: cache definitions
};

DefinitionsManager::DefinitionsManager() : impl(std::make_unique<DefinitionsManager::Impl>()) {}

DefinitionsManager::DefinitionsManager(DefinitionsManager&& manager) = default;
DefinitionsManager::~DefinitionsManager()                            = default;

MethodDefinition MethodDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name    = String::ParseOffset(reader);
    auto idx     = reader.ReadU32();
    auto sigIdx  = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    // uint32_t codeOffs = reader.ReadU32();
    uint32_t codeOffs = 0;

    return MethodDefinition(fileId, name, idx, sigIdx, declIdx, codeOffs);
}

MethodDefinition MethodDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    auto position = session.CbcFileOf(fileId).GetMethodDefOffs(offset);
    IO::StreamFileReader reader(*session.FileOf(fileId), position);
    return Parse(fileId, reader);
}

MethodDefinition MethodDefinition::Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier)
{
    return MethodDefinition::Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

TypeDefinition TypeDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    auto position = session.CbcFileOf(fileId).GetTypeDefOffs(offset);
    IO::StreamFileReader reader(*session.FileOf(fileId), position);
    return Parse(fileId, reader);
}

TypeDefinition TypeDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::ParseOffset(reader);
    auto idx  = reader.ReadU32();
    auto tk   = reader.ReadU32();

    auto superCount = reader.ReadU32();

    std::vector<uint32_t> supers;
    supers.reserve(superCount);

    char* supersRaw = reinterpret_cast<char*>(supers.data());
    reader.Read(supersRaw, superCount * sizeof(uint32_t));

    return TypeDefinition(fileId, name, idx, TypeKind(static_cast<TypeKind::Value>(tk)), std::move(supers));
}

TypeDefinition TypeDefinition::Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

FieldDefinition FieldDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name    = String::ParseOffset(reader);
    auto idx     = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    auto typeIdx = reader.ReadU32();

    return FieldDefinition(fileId, name, idx, declIdx, typeIdx);
}

} // namespace Symlevel
