#include "definitions.h"
#include "engine/identifiers.h"
#include "engine/symlevel/access_kind.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/sequence.h"
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

    auto fieldIndex     = FieldIndex::Read(reader, fileId);
    auto instanceFields = OffsetSequence<FieldDefinition>::Parse(reader, fileId);

    auto test = [parsedFlags](uint32_t bits) { return (parsedFlags & bits) != 0; };

    TypeFlags flags;
    flags = flags.With(TypeKind::CLASS);

    if (test(0x001))
        flags = flags.With(AccessKind::PUBLIC);
    if (test(0x002))
        flags = flags.Or(TypeFlag::FINAL);
    if (test(0x004))
        flags = flags.Or(TypeFlag::ABSTRACT);
    if (test(0x008))
        flags = flags.Or(TypeFlag::SEALED);
    if (test(0x010))
        flags = flags.With(TypeKind::INTERFACE);
    if (test(0x020))
        flags = flags.With(TypeKind::LAMBDA);
    if (test(0x040))
        flags = flags.With(TypeKind::RECORD);
    if (test(0x080))
        flags = flags.Or(TypeFlag::AOT);
    if (test(0x100))
        flags = flags.Or(TypeFlag::PATCH);
    if (test(0x200))
        flags = flags.With(TypeKind::ENUM);

    TypeDefinition::Content def {
        .identifier      = Engine::Identifier(offset, fileId),
        .name            = name,
        .methods         = std::move(methodIndex),
        .fields          = std::move(fieldIndex),
        .virtualMethods  = dynMethods,
        .instanceFields  = instanceFields,
        .superOrEnumType = superType,
        .flags           = flags,
        .arity           = 0,
        .enumKind        = EnumKind::NOT_ENUM,
    };


    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.interfaces = RefSequence<Term>::Parse(reader, fileId, regionId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            case 0x6:
                def.unionFields = RefSequence<Term>::Parse(reader, fileId, regionId);
                def.enumKind = EnumKind::UNION;
                break;
            case 0x7: def.enumKind = EnumKind::OPTION0; break;
            case 0x8: def.enumKind = EnumKind::OPTION1; break;
            case 0x9: def.enumKind = EnumKind::PRIMITIVE; break;
            default:  FATAL("unexpected tag: %d", tag);
        }
    }
    return TypeDefinition(std::move(def));
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
    if (test(0x20))
        flags = flags.Or(FieldFlag::AOT);

    FieldDefinition::Content content {
        .identifier = Engine::Identifier(offset, fileId),
        .nameOffset = nameOffset,
        .fieldType = fieldType,
        .flags = flags,
    };

    return FieldDefinition(std::move(content));
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
    auto typeNameOffset = Offset<String>(reader.ReadU32());
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

    if (test(0x0004))
        flags = flags.Or(MethodFlag::STATIC);
    if (test(0x0008))
        flags = flags.Or(MethodFlag::FINAL);
    if (test(0x0010))
        flags = flags.Or(MethodFlag::FOREIGN);
    if (test(0x0020))
        flags = flags.Or(MethodFlag::ABSTRACT);
    if (test(0x0040))
        flags = flags.Or(MethodFlag::MUT);
    if (test(0x0080))
        flags = flags.Or(MethodFlag::VIRTUAL);
    if (test(0x0100))
        flags = flags.Or(MethodFlag::AOT);
    if (test(0x0200))
        flags = flags.Or(MethodFlag::PKG_INIT);
    if (test(0x0400))
        flags = flags.Or(MethodFlag::LIT_INIT);
    if (test(0x0800))
        flags = flags.Or(MethodFlag::SRET);
    if (test(0x1000))
        flags = flags.Or(MethodFlag::HAS_THIS_TI);
    if (test(0x2000))
        flags = flags.Or(MethodFlag::HAS_OUTER_TI);

    MethodDefinition::Content def{ Engine::Identifier(offset, fileId), signature, typeNameOffset, nameOffset, flags};

    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.code = Engine::Identifier(Offset<Code>(reader.ReadULEB()), fileId); break;
            case 0x2: def.sourceFullName = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x3: def.sourceFile = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x4: def.linkageName = Engine::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            default:  FATAL("unexpected tag: %d", tag);
        }
    }

    return def;
}

MethodRefFlags MethodDefinition::GetABIFlags() const
{
    MethodRefFlags flags;
    auto defFlags = content.flags;
    if (defFlags.Is(MethodFlag::SRET))
        flags = flags.Or(MethodRefFlag::SRET);
    if (defFlags.Is(MethodFlag::HAS_OUTER_TI))
        flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (defFlags.Is(MethodFlag::HAS_THIS_TI))
        flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (defFlags.Is(MethodFlag::MUT))
        flags = flags.Or(MethodRefFlag::MUT);
    if (content.arity > 0)
        flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    return flags;
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
