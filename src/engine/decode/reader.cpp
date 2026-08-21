#include "reader.h"
#include "engine/image/flags.h"
#include "engine/image/io/stream_file_reader.h"
#include "engine/terms.h"
#include <cstdint>

namespace Decode {

using namespace Image;

std::vector<ExceptionRegion> Reader::GetExceptionRegions(Engine::Session& session, Code const& code)
{
    IO::StreamFileReader reader(*session.FileOf(code.rawExTable.fileId), code.rawExTable.start);
    std::vector<ExceptionRegion> regions;
    while (reader.Position() < code.rawExTable.end) {
        regions.emplace_back(ExceptionRegion {
            .start  = reader.ReadULEB(),
            .end    = reader.ReadULEB(),
            .target = reader.ReadULEB(),
        });
    }
    return regions;
}

std::vector<LivenessInfo> Reader::GetLivenessInfo(Engine::Session& session, Code const& code)
{
    auto& rawLivenessInfo = code.rawLivenessInfo;
    IO::StreamFileReader reader(*session.FileOf(rawLivenessInfo.fileId), rawLivenessInfo.start);

    std::vector<LivenessInfo> livenessInfo;
    while (reader.Position() < rawLivenessInfo.end) {
        LivenessInfo info = {
            .cbcPos  = reader.ReadULEB(),
            .regMask = reader.ReadU16(),
        };

        uint32_t slotsN = reader.ReadULEB();
        std::vector<uint32_t> slots;
        slots.reserve(slotsN);

        for (uint32_t idx = 0; idx < slotsN; idx++) {
            slots.push_back(reader.ReadULEB());
        }
        info.refSlotNums = std::move(slots);

        uint32_t pairsN = reader.ReadULEB();
        std::vector<std::pair<uint32_t, uint32_t>> mutPairs;
        mutPairs.reserve(pairsN);

        for (uint32_t idx = 0; idx < pairsN; idx++) {
            mutPairs.push_back(std::pair(reader.ReadULEB(), reader.ReadULEB()));
        }
        info.mutPairs = std::move(mutPairs);

        livenessInfo.push_back(std::move(info));
    }

    return livenessInfo;
}

std::vector<StackPtrsInfo> Reader::GetStackPtrsInfo(Engine::Session& session, Code const& code)
{
    auto& rawStackPtrsInfo = code.rawStackPtrsInfo;
    IO::StreamFileReader reader(*session.FileOf(rawStackPtrsInfo.fileId), rawStackPtrsInfo.start);

    std::vector<StackPtrsInfo> stackPtrsInfo;
    while (reader.Position() < rawStackPtrsInfo.end) {
        StackPtrsInfo info = {
            .cbcPos = reader.ReadULEB(),
        };

        uint32_t resourcesN = reader.ReadULEB();
        std::vector<uint32_t> resources;
        resources.reserve(resourcesN);

        for (uint32_t idx = 0; idx < resourcesN; idx++) {
            resources.push_back(reader.ReadULEB());
        }
        info.resources = std::move(resources);

        stackPtrsInfo.push_back(std::move(info));
    }

    return stackPtrsInfo;
}

template <> MethodReference Reader::Read(Engine::Session& session, Identifier<MethodReference> id)
{
    auto fileId = id.GetFileId();
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + id.GetOffset()
    );

    auto nameOffset   = Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto parsedFlags  = reader.ReadU8();
    auto refTypeIdx   = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto methodSigIdx = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    MethodRefFlags flags;
    if (parsedFlags & 0x1)
        flags = flags.Or(MethodRefFlag::SRET);
    if (parsedFlags & 0x2)
        flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (parsedFlags & 0x4)
        flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (parsedFlags & 0x8)
        flags = flags.Or(MethodRefFlag::MUT);
    if (parsedFlags & 0x10)
        flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    if (parsedFlags & 0x20)
        flags = flags.Or(MethodRefFlag::AOT);
    if (parsedFlags & 0x40)
        flags = flags.Or(MethodRefFlag::REC_RECEIVER);
    if (parsedFlags & 0x80)
        flags = flags.Or(MethodRefFlag::REF_RECEIVER);

    static constexpr auto NIL_ID = RefId<Term>((uint16_t)Engine::TermKind::NIL);

    RefIdentifier<Term> tvars(NIL_ID, fileId);
    if (flags.Is(MethodRefFlag::HAS_FTVARS)) {
        tvars = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    }

    return { nameOffset, refTypeIdx, methodSigIdx, tvars, flags };
}

template <> FieldReference Reader::Read(Engine::Session& session, Identifier<FieldReference> id)
{
    auto fileId = id.GetFileId();
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + id.GetOffset()
    );

    auto nameOffset   = Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto refTypeIdx   = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto fieldTypeIdx = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    auto isRecord = reader.ReadU8() != 0;

    return { nameOffset, refTypeIdx, fieldTypeIdx, isRecord };
}

template <typename Reference>
inline static Reference ParseReference(Engine::Session& session, RefIdentifier<Reference> identifier)
{
    return Reader::Read(session, session.Decoder().Resolve(identifier));
}

MethodReference Reader::Read(Engine::Session& session, RefIdentifier<MethodReference> identifier)
{
    return ParseReference(session, identifier);
}

FieldReference Reader::Read(Engine::Session& session, RefIdentifier<FieldReference> identifier)
{
    return ParseReference(session, identifier);
}

template <> String Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetStringSectionOffs() + offset);

    uint32_t size = reader.ReadULEB();
    auto mem      = static_cast<char*>(session.Allocator().Allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

template <> String Reader::Read(Engine::Session& session, Image::Identifier<String> ident)
{
    return Reader::Read(session, ident.GetFileId(), ident.GetOffset());
}

template <> Code Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetCodeSectionOffs() + offset);

    uint32_t untypedSlotCount = reader.ReadULEB();

    uint32_t stackAllocSigsCount = reader.ReadULEB();
    uint32_t* stackAllocSigs =
        static_cast<uint32_t*>(session.Allocator().Allocate(stackAllocSigsCount * sizeof(uint32_t), alignof(uint32_t)));
    for (size_t i = 0; i < stackAllocSigsCount; i++) {
        stackAllocSigs[i] = reader.ReadULEB();
    }

    uint32_t ohmSlotCount = reader.ReadULEB(); // TODO: impl

    uint8_t usedNonVolIRegMask       = reader.ReadU8();
    uint8_t usedNonVolFRegMask       = reader.ReadU8();
    uint32_t maxCalleeStackArgsCount = reader.ReadULEB();

    bool mayHaveNativeCalls = static_cast<bool>(reader.ReadU8());

    uint32_t codeSize       = reader.ReadULEB();
    uint32_t literalsOffset = reader.ReadULEB(); // TODO: remove

    auto codePtr = static_cast<uint8_t*>(session.Allocator().Allocate(codeSize, alignof(uint8_t)));
    reader.Read(codePtr, codeSize);

    uint32_t exTableSize  = reader.ReadULEB();
    uint32_t exTableStart = reader.Position();
    reader.Advance(exTableSize);

    uint32_t livenessInfoSize  = reader.ReadULEB();
    uint32_t livenessInfoStart = reader.Position();
    reader.Advance(livenessInfoSize);

    uint32_t stackPtrsInfoSize  = reader.ReadULEB();
    uint32_t stackPtrsInfoStart = reader.Position();
    reader.Advance(stackPtrsInfoSize);

    return Code(
        untypedSlotCount,
        stackAllocSigsCount,
        stackAllocSigs,
        ohmSlotCount,
        usedNonVolIRegMask,
        usedNonVolFRegMask,
        maxCalleeStackArgsCount,
        mayHaveNativeCalls,
        codeSize,
        codePtr,
        { fileId, exTableStart, exTableStart + exTableSize },
        { fileId, livenessInfoStart, livenessInfoStart + livenessInfoSize },
        { fileId, stackPtrsInfoStart, stackPtrsInfoStart + stackPtrsInfoSize }
    );
}

template <> TypeDefinition Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<TypeDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTypeDefSectionOffs() + offset);

    auto name        = Image::Identifier(Offset<String>(reader.ReadU32()), fileId);
    auto regionId    = reader.ReadU8();
    auto parsedFlags = reader.ReadU16();
    auto superType   = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

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
        .identifier      = Image::Identifier(offset, fileId),
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
                def.enumKind    = EnumKind::UNION;
                break;
            case 0x7: def.enumKind = EnumKind::OPTION0; break;
            case 0x8: def.enumKind = EnumKind::OPTION1; break;
            case 0x9: def.enumKind = EnumKind::PRIMITIVE; break;
            default:  FATAL("unexpected tag: %d", tag);
        }
    }
    return TypeDefinition(std::move(def));
}

template <> TypeDefinition Reader::Read(Engine::Session& session, Image::Identifier<TypeDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> FieldDefinition Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<FieldDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldDefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto regionId     = reader.ReadU8();
    auto fieldTypeIdx = reader.ReadULEB();
    auto parsedFlags  = reader.ReadU8();

    // TODO parse const value
    auto tag = reader.ReadU8();
    ASSERTION(tag == 0, "Const value is not supported yet");

    auto fieldType = Image::RefIdentifier(RefId<Term>(fieldTypeIdx), fileId);

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
        .identifier = Image::Identifier(offset, fileId),
        .nameOffset = nameOffset,
        .fieldType  = fieldType,
        .flags      = flags,
    };

    return FieldDefinition(std::move(content));
}

template <>
MethodDefinition Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<MethodDefinition> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodDefSectionOffs() + offset);

    auto nameOffset     = Offset<String>(reader.ReadU32());
    auto typeNameOffset = Offset<String>(reader.ReadU32());
    auto regionId       = reader.ReadU8();
    auto signature      = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto parsedFlags    = reader.ReadU16();

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

    MethodDefinition::Content def { Image::Identifier(offset, fileId), signature, typeNameOffset, nameOffset, flags };

    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.code = Image::Identifier(Offset<Code>(reader.ReadULEB()), fileId); break;
            case 0x2: def.sourceFullName = Image::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x3: def.sourceFile = Image::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x4: def.linkageName = Image::Identifier(Offset<String>(reader.ReadULEB()), fileId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            default:  FATAL("unexpected tag: %d", tag);
        }
    }

    return MethodDefinition(std::move(def));
}

template <> FieldDefinition Reader::Read(Engine::Session& session, Image::Identifier<FieldDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> Code Reader::Read(Engine::Session& session, Image::Identifier<Code> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> MethodDefinition Reader::Read(Engine::Session& session, Image::Identifier<MethodDefinition> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

template <> Extension Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<Extension> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetTypeDefSectionOffs() + offset);

    auto extendedType = Image::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    auto dynMethods  = Reader::ReadOffsSeq<MethodDefinition>(reader, fileId);

    Extension::Content def {
        .identifier      = Image::Identifier(offset, fileId),
        .virtualMethods  = dynMethods,
        .extendedType    = extendedType,
        .arity           = 0,
    };

    for (auto tag = reader.ReadU8(); tag != 0; tag = reader.ReadU8()) {
        switch (tag) {
            case 0x1: def.interfaces = Reader::ReadRefSeq<Term>(reader, fileId); break;
            case 0x5: def.arity = reader.ReadULEB(); break; // TODO: check range
            default:  FATAL("unexpected tag: %d", tag);
        }
    }
    return Extension(std::move(def));
}

template <> Extension Reader::Read(Engine::Session& session, Image::Identifier<Extension> identifier)
{
    return Reader::Read(session, identifier.GetFileId(), identifier.GetOffset());
}

Image::RegionData Reader::ReadRegion(Image::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    uint16_t typeIdxSize = reader.ReadU16(); // TODO: remove
    uint32_t typeIdxOffs = reader.ReadU32(); // TODO: remove

    uint16_t methodIndexSize = reader.ReadULEB();
    uint32_t methodIndexOffs = reader.ReadU32();

    uint16_t fieldIndexSize = reader.ReadULEB();
    uint32_t fieldIndexOffs = reader.ReadU32();

    uint32_t termIndexSize = reader.ReadULEB();
    uint32_t termIndexOffs = reader.ReadU32();

    OffsetPool<MethodReference> methods(fileId, methodIndexOffs, methodIndexSize);
    OffsetPool<FieldReference> fields(fileId, fieldIndexOffs, fieldIndexSize);
    OffsetPool<Term, Engine::FIRST_NON_PRIMITIVE> terms(fileId, termIndexOffs, termIndexSize);

    return RegionData(methods, fields, terms);
}

} // namespace Decode
