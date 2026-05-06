#include "definitions.h"
#include "engine/identifiers.h"
#include "engine/symlevel/offset_sequence.h"
#include "reader.h"
#include <optional>

namespace Symlevel {

class DefinitionsManager::Impl {
    // TODO: cache definitions
};

DefinitionsManager::DefinitionsManager() : impl(std::make_unique<DefinitionsManager::Impl>()) {}

DefinitionsManager::DefinitionsManager(DefinitionsManager&& manager) = default;
DefinitionsManager::~DefinitionsManager()                            = default;

TypeDefinition TypeDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTypeDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto flags        = reader.ReadULEB();
    auto importTable  = reader.ReadU32();
    auto pkgIndex     = reader.ReadU16();
    auto superTypeIdx = reader.ReadULEB();

    auto methodIndex = MethodIndex::Read(reader, fileId);

    auto dynMethods = OffsetSequence<MethodDefinition>::Parse(reader, fileId);

    auto fieldIndex = FieldIndex::Read(reader, fileId);

    return TypeDefinition { Engine::Identifier(offset, fileId),
                            Engine::Identifier(nameOffset, fileId),
                            std::move(methodIndex),
                            std::move(fieldIndex),
                            dynMethods,
                            Engine::RefIdentifier(RefId<Term>(0, superTypeIdx), fileId) };
}

TypeDefinition TypeDefinition::Resolve(Engine::Session& session, Engine::Identifier<TypeDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

String TypeDefinition::ParseName(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTypeDefSectionOffs() + offset);

    auto nameOffset = Offset<String>(reader.ReadU32());
    return Reader::Read(session, fileId, nameOffset);
}

FieldDefinition FieldDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto refTypeIdx   = reader.ReadULEB();
    auto fieldTypeIdx = reader.ReadULEB();
    auto flags        = reader.ReadU8();

    // TODO parse const value
    auto tag = reader.ReadU8();
    ASSERTION(tag == 0, "Const value is not supported yet");

    auto refType   = Engine::RefIdentifier(RefId<Term>(0, refTypeIdx), fileId);
    auto fieldType = Engine::RefIdentifier(RefId<Term>(0, fieldTypeIdx), fileId);

    return FieldDefinition(
        Engine::Identifier<FieldDefinition>(offset, fileId), nameOffset, refType, fieldType, FieldFlags(flags), {}
    );
}

FieldDefinition FieldDefinition::Resolve(Engine::Session& session, Engine::Identifier<FieldDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

String FieldDefinition::ParseName(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldDefSectionOffs() + offset);

    auto nameOffset = Offset<String>(reader.ReadU32());
    return Reader::Read(session, fileId, Offset<String>(nameOffset));
}

MethodDefinition MethodDefinition::Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto signature    = Engine::RefIdentifier(RefId<Term>(0, reader.ReadU32()), fileId);
    auto specialFlags = reader.ReadU8();
    auto flags        = reader.ReadULEB();
    auto methodIdx    = reader.ReadSLEB();

    std::optional<Engine::Identifier<Code>> code = std::nullopt;

    // TODO: use enum and support all tags
    bool stop = false;
    while (!stop) {
        auto tag = reader.ReadU8();
        switch (tag) {
            case 0: stop = true; break;
            case 1: code = Engine::Identifier(Offset<Code>(reader.ReadU32()), fileId); break;

            default: FATAL("unexpected tag: %d", tag); std::exit(2);
        }
    }

    return MethodDefinition(Engine::Identifier(offset, fileId), nameOffset, signature, code);
}

MethodDefinition MethodDefinition::Resolve(Engine::Session& session, Engine::Identifier<MethodDefinition> identifier)
{
    return Parse(session, identifier.GetFileId(), identifier.GetOffset());
}

String MethodDefinition::ParseName(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto nameOffset = Offset<String>(reader.ReadU32());
    return Reader::Read(session, fileId, Offset<String>(nameOffset));
}

} // namespace Symlevel
