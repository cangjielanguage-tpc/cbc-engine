#include "references.h"
#include "engine/symlevel/flags.h"
#include "engine/terms.h"
#include "io/stream_file_reader.h"
#include "region_data.h"
#include <cstdint>

namespace Symlevel {

MethodReference ParseReference(
    Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset, uint8_t region
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + offset);

    auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto parsedFlags  = reader.ReadU8();
    auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(region, reader.ReadULEB()), fileId);
    auto methodSigIdx = Engine::RefIdentifier(RefId<Term>(region, reader.ReadULEB()), fileId);

    MethodRefFlags flags;
    if (parsedFlags & 0x1) flags = flags.Or(MethodRefFlag::SRET);
    if (parsedFlags & 0x2) flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (parsedFlags & 0x4) flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (parsedFlags & 0x8) flags = flags.Or(MethodRefFlag::HAS_BASE_PTR);
    if (parsedFlags & 0x10) flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    if (parsedFlags & 0x20) flags = flags.Or(MethodRefFlag::AOT);

    static constexpr auto NIL_ID = RefId<Term>(0, (uint16_t) Engine::TermKind::NIL);

    Engine::RefIdentifier<Term> tvars(NIL_ID, fileId);
    if (flags.Is(MethodRefFlag::HAS_FTVARS)) {
        tvars = Engine::RefIdentifier(RefId<Term>(region, reader.ReadULEB()), fileId);
    }

    return { nameOffset, refTypeIdx, methodSigIdx, tvars, flags };
}

FieldReference ParseReference(
    Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset, uint8_t region
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + offset);

    auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(region, reader.ReadULEB()), fileId);
    auto fieldTypeIdx = Engine::RefIdentifier(RefId<Term>(region, reader.ReadULEB()), fileId);

    auto isRecord = reader.ReadU8() != 0;

    return { nameOffset, refTypeIdx, fieldTypeIdx, isRecord };
}

template <typename Reference>
inline static Reference ParseReference(Engine::Session& session, Engine::RefIdentifier<Reference> identifier)
{
    auto& file       = session.CbcFileOf(identifier.GetFileId());
    auto& raf        = session.FileOf(identifier.GetFileId());
    auto& regionData = file.GetRegionData();
    auto offset      = regionData.Query(session, identifier.GetIndex());
    return ParseReference(session, identifier.GetFileId(), offset, identifier.GetIndex().GetRegion());
}

MethodReference MethodReference::Parse(Engine::Session& session, Engine::RefIdentifier<MethodReference> identifier)
{
    return ParseReference(session, identifier);
}

FieldReference FieldReference::Parse(Engine::Session& session, Engine::RefIdentifier<FieldReference> identifier)
{
    return ParseReference(session, identifier);
}

} // namespace Symlevel
