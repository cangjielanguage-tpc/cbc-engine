#include "definitions.h"
#include "engine/identifiers.h"
#include "engine/symlevel/access_kind.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/offset_sequence.h"
#include "engine/symlevel/type_kind.h"
#include "reader.h"
#include <cstdint>
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

    auto name        = Engine::Identifier(Offset<String>(reader.ReadU32()), fileId);
    auto regionId    = reader.ReadU8();
    auto parsedFlags = reader.ReadU16();
    auto superType   = Engine::RefIdentifier(RefId<Term>(regionId, reader.ReadULEB()), fileId);

    auto methodIndex = MethodIndex::Read(reader, fileId);
    auto dynMethods  = OffsetSequence<MethodDefinition>::Parse(reader, fileId);
    auto fieldIndex  = FieldIndex::Read(reader, fileId);

    auto test = [parsedFlags](uint32_t bits) { return (parsedFlags & bits) != 0; };

    TypeFlags flags;
    flags = flags.With(TypeKind::CLASS);

    if (test(0x01))
        flags = flags.With(AccessKind::PUBLIC);
    if (test(0x02))
        flags = flags.Or(TypeFlag::FINAL);
    if (test(0x04))
        flags = flags.Or(TypeFlag::ABSTRACT);
    if (test(0x08))
        flags = flags.Or(TypeFlag::SEALED);
    if (test(0x10))
        flags = flags.With(TypeKind::INTERFACE);
    if (test(0x40))
        flags = flags.With(TypeKind::RECORD);
    if (test(0x80))
        flags = flags.Or(TypeFlag::AOT);

    return TypeDefinition { Engine::Identifier(offset, fileId),
                            name,
                            std::move(methodIndex),
                            std::move(fieldIndex),
                            dynMethods,
                            superType,
                            flags };
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
    auto regionId     = reader.ReadU8();
    auto fieldTypeIdx = reader.ReadULEB();
    auto parsedFlags  = reader.ReadU8();

    // TODO parse const value
    auto tag = reader.ReadU8();
    ASSERTION(tag == 0, "Const value is not supported yet");

    auto fieldType = Engine::RefIdentifier(RefId<Term>(regionId, fieldTypeIdx), fileId);

    auto test = [parsedFlags](uint32_t bits) { return (parsedFlags & bits) != 0; };

    auto testMask = [parsedFlags](uint32_t bits, uint32_t mask) { return (parsedFlags & mask) == bits; };

    FieldFlags flags;

    if (testMask(0b01, 0b11))
        flags = flags.With(AccessKind::PUBLIC);
    if (testMask(0b10, 0b11))
        flags = flags.With(AccessKind::PRIVATE);
    if (testMask(0b11, 0b11))
        flags = flags.With(AccessKind::PROTECTED);

    if (test(0x04))
        flags = flags.Or(FieldFlag::STATIC);
    if (test(0x08))
        flags = flags.Or(FieldFlag::FINAL);

    return FieldDefinition(
        Engine::Identifier<FieldDefinition>(offset, fileId), nameOffset, fieldType, flags, {}
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

    auto nameOffset  = Offset<String>(reader.ReadU32());
    auto regionId    = reader.ReadU8();
    auto signature   = Engine::RefIdentifier(RefId<Term>(regionId, reader.ReadULEB()), fileId);
    auto parsedFlags = reader.ReadU16();

    auto test = [parsedFlags](uint32_t bits) { return (parsedFlags & bits) == bits; };

    auto testMask = [parsedFlags](uint32_t bits, uint32_t mask) { return (parsedFlags & mask) == bits; };

    MethodFlags flags;

    if (testMask(0b01, 0b11))
        flags = flags.With(AccessKind::PUBLIC);
    if (testMask(0b10, 0b11))
        flags = flags.With(AccessKind::PRIVATE);
    if (testMask(0b11, 0b11))
        flags = flags.With(AccessKind::PROTECTED);

    if (test(0x004))
        flags = flags.Or(MethodFlag::STATIC);
    if (test(0x008))
        flags = flags.Or(MethodFlag::FINAL);
    if (test(0x010))
        flags = flags.Or(MethodFlag::FOREIGN);
    if (test(0x020))
        flags = flags.Or(MethodFlag::ABSTRACT);
    if (test(0x040))
        flags = flags.Or(MethodFlag::MUT);
    if (test(0x080))
        flags = flags.Or(MethodFlag::VIRTUAL);
    if (test(0x100))
        flags = flags.Or(MethodFlag::AOT);

    MethodDefinition def(Engine::Identifier(offset, fileId), nameOffset, signature, flags);

    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.code = Engine::Identifier(Offset<Code>(reader.ReadULEB()), fileId); break;
            case 0x2: def.sourceFullName = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x3: def.sourceFile = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x4: def.linkageName = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            default:  FATAL("unexpected tag: %d", tag); std::exit(2);
        }
    }

    return def;
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
