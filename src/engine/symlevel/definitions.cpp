#include "engine/decode/decoder.h"
#include "engine/symlevel/access_kind.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include <cstdint>
#include <optional>

namespace Symlevel {

template <> TypeDefinition Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<TypeDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTypeDefSectionOffs() + offset);

    auto name        = Symlevel::Identifier(Offset<String>(reader.ReadU32()), fileId);
    auto regionId    = reader.ReadU8();
    auto parsedFlags = reader.ReadU16();
    auto superType   = Symlevel::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    auto methodIndex = Decode::ReadIndex(reader, fileId);
    auto dynMethods  = Reader::ReadOffsSeq<MethodDefinition>(reader, fileId);

    auto fieldIndex     = Decode::ReadIndex(reader, fileId);
    auto instanceFields = Reader::ReadOffsSeq<FieldDefinition>(reader, fileId);

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
        .identifier      = Symlevel::Identifier(offset, fileId),
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
            case 0x1: def.interfaces = Reader::ReadRefSeq<Term>(reader, fileId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            case 0x6:
                def.unionFields = Reader::ReadRefSeq<Term>(reader, fileId);
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

template <> TypeDefinition Reader::Read(Engine::Session& session, Symlevel::Identifier<TypeDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> FieldDefinition Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<FieldDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto regionId     = reader.ReadU8();
    auto fieldTypeIdx = reader.ReadULEB();
    auto parsedFlags  = reader.ReadU8();

    // TODO parse const value
    auto tag = reader.ReadU8();
    ASSERTION(tag == 0, "Const value is not supported yet");

    auto fieldType = Symlevel::RefIdentifier(RefId<Term>(fieldTypeIdx), fileId);

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
        .identifier = Symlevel::Identifier(offset, fileId),
        .nameOffset = nameOffset,
        .fieldType  = fieldType,
        .flags      = flags,
    };

    return FieldDefinition(std::move(content));
}

template <> FieldDefinition Reader::Read(Engine::Session& session, Symlevel::Identifier<FieldDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> MethodDefinition Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<MethodDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto nameOffset  = Offset<String>(reader.ReadU32());
    auto typeNameOffset = Offset<String>(reader.ReadU32());
    auto regionId    = reader.ReadU8();
    auto signature      = Symlevel::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
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
    if (test(0x4000))
        flags = flags.Or(MethodFlag::REC_RECEIVER);
    if (test(0x8000))
        flags = flags.Or(MethodFlag::REF_RECEIVER);

    MethodDefinition::Content def {
        Symlevel::Identifier(offset, fileId), signature, typeNameOffset, nameOffset, flags
    };

    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.code = Symlevel::Identifier(Offset<Code>(reader.ReadULEB()), fileId); break;
            case 0x2: def.sourceFullName = Symlevel::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x3: def.sourceFile = Symlevel::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x4: def.linkageName = Symlevel::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            default:  FATAL("unexpected tag: %d", tag);
        }
    }

    return MethodDefinition(std::move(def));
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
    if (defFlags.Is(MethodFlag::REC_RECEIVER))
        flags = flags.Or(MethodRefFlag::REC_RECEIVER);
    if (defFlags.Is(MethodFlag::REF_RECEIVER))
        flags = flags.Or(MethodRefFlag::REF_RECEIVER);

    return flags;
}

template <> MethodDefinition Reader::Read(Engine::Session& session, Symlevel::Identifier<MethodDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

} // namespace Symlevel
