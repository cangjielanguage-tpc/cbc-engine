#include "definitions.h"

namespace Symlevel {

class DefinitionsManager::Impl {
    // TODO: cache definitions
};

DefinitionsManager::DefinitionsManager() : impl(std::make_unique<DefinitionsManager::Impl>()) {}

DefinitionsManager::DefinitionsManager(DefinitionsManager&& manager) = default;
DefinitionsManager::~DefinitionsManager()                            = default;

MethodDefinition MethodDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    auto position = session.CbcFileOf(fileId).GetMethodDefOffs(offset);
    IO::StreamFileReader reader(*session.FileOf(fileId), position);
    auto nameOffs = Offset<String>(reader.ReadU32());
    auto _        = Offset<String>(reader.ReadU32()); // FIXME: signature encoding
    auto codeOffs = Offset<Code>(reader.ReadU32());

    return MethodDefinition(Engine::Identifier<MethodDefinition>(offset, fileId), nameOffs, 0, codeOffs);
}

MethodDefinition MethodDefinition::Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier)
{
    return MethodDefinition::Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

TypeDefinition TypeDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    auto position = session.CbcFileOf(fileId).GetTypeDefOffs(offset);
    IO::StreamFileReader reader(*session.FileOf(fileId), position);
    auto name       = Offset<String>(reader.ReadU32());
    auto _kind      = reader.ReadU32();
    auto _superName = reader.ReadU32();

    auto methodsLength = reader.ReadU32();
    auto methodsOffset = reader.Position();
    reader.Advance(methodsLength);

    return TypeDefinition(Engine::Identifier<TypeDefinition>(offset, fileId), name, methodsLength, methodsOffset);
}

TypeDefinition TypeDefinition::Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

FieldDefinition FieldDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name    = Offset<String>(reader.ReadU32());
    auto idx     = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    auto typeIdx = reader.ReadU32();

    return FieldDefinition(fileId, name, idx, declIdx, typeIdx);
}

} // namespace Symlevel
