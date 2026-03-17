#include "definitions.h"

namespace Symlevel {

class DefinitionsManager::Impl {
    // TODO: cache definitions
};

DefinitionsManager::DefinitionsManager() : impl(std::make_unique<DefinitionsManager::Impl>()) {}

DefinitionsManager::DefinitionsManager(DefinitionsManager&& manager) = default;
DefinitionsManager::~DefinitionsManager()                            = default;

TypeDefinition TypeDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    auto& raf = *session.FileOf(fileId);
    IO::StreamFileReader reader(raf, session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto name         = Offset<String>(reader.ReadU32());
    auto flags        = reader.ReadULEB();
    auto importTable  = reader.ReadU32();
    auto pkgIndex     = reader.ReadU16();
    auto superTypeIdx = reader.ReadULEB();

    auto methodIndex = MethodIndex::Read(reader, fileId);
    auto fieldIndex  = FieldIndex::Read(reader, fileId);

    return TypeDefinition(
        Engine::Identifier<TypeDefinition>(offset, fileId), name, std::move(methodIndex), std::move(fieldIndex)
    );
}

TypeDefinition TypeDefinition::Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

FieldDefinition FieldDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset)
{
    auto& raf = *session.FileOf(fileId);
    IO::StreamFileReader reader(raf, session.CbcFileOf(fileId).GetFieldDefSectionOffs() + offset);

    auto name    = Offset<String>(reader.ReadU32());
    auto idx     = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    auto typeIdx = reader.ReadU32();

    return FieldDefinition(Engine::Identifier<FieldDefinition>(offset, fileId), name, idx, declIdx, typeIdx);
}

FieldDefinition FieldDefinition::Resolve(Engine::Session& session, Engine::Identifier<FieldDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

MethodDefinition MethodDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    auto& raf = *session.FileOf(fileId);
    IO::StreamFileReader reader(raf, session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto methodSigIdx = reader.ReadU32();
    auto specialFlags = reader.ReadU8();
    auto flags        = reader.ReadULEB();
    auto methodIdx    = reader.ReadSLEB();

    std::optional<Offset<Code>> codeOffs;

    // TODO: use enum and support all tags
    while (true) {
        auto tag = reader.ReadU8();
        switch (tag) {
            case 0: goto tags_end;
            case 1: codeOffs.emplace(Offset<Code>(reader.ReadU32())); break;

            default: ASSERT(false); std::exit(1);
        }
    }

tags_end:

    return MethodDefinition(Engine::Identifier<MethodDefinition>(offset, fileId), nameOffset, 0, codeOffs);
}

MethodDefinition MethodDefinition::Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

} // namespace Symlevel
