#include "references.h"
#include "engine/identifiers.h"
#include "engine/symlevel/flags.h"
#include "engine/symlevel/reader.h"
#include "engine/terms.h"
#include "io/stream_file_reader.h"
#include "region_data.h"
#include <cstdint>

namespace Symlevel {

template <> MethodReference Reader::Read(Engine::Session& session, Engine::Identifier<MethodReference> id)
{
    auto fileId = id.GetFileId();
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + id.GetOffset()
    );

    auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto parsedFlags  = reader.ReadU8();
    auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto methodSigIdx = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    MethodRefFlags flags;
    if (parsedFlags & 0x1) flags = flags.Or(MethodRefFlag::SRET);
    if (parsedFlags & 0x2) flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (parsedFlags & 0x4) flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (parsedFlags & 0x8)
        flags = flags.Or(MethodRefFlag::MUT);
    if (parsedFlags & 0x10) flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    if (parsedFlags & 0x20) flags = flags.Or(MethodRefFlag::AOT);
    if (parsedFlags & 0x40)
        flags = flags.Or(MethodRefFlag::REC_RECEIVER);
    if (parsedFlags & 0x80)
        flags = flags.Or(MethodRefFlag::REF_RECEIVER);

    static constexpr auto NIL_ID = RefId<Term>((uint16_t)Engine::TermKind::NIL);

    Engine::RefIdentifier<Term> tvars(NIL_ID, fileId);
    if (flags.Is(MethodRefFlag::HAS_FTVARS)) {
        tvars = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    }

    return { nameOffset, refTypeIdx, methodSigIdx, tvars, flags };
}

template <> FieldReference Reader::Read(Engine::Session& session, Engine::Identifier<FieldReference> id)
{
    auto fileId = id.GetFileId();
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + id.GetOffset()
    );

    auto nameOffset   = Engine::Identifier<String>(Offset<String>(reader.ReadU32()), fileId);
    auto refTypeIdx   = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);
    auto fieldTypeIdx = Engine::RefIdentifier(RefId<Term>(reader.ReadULEB()), fileId);

    auto isRecord = reader.ReadU8() != 0;

    return { nameOffset, refTypeIdx, fieldTypeIdx, isRecord };
}

template <typename Reference>
inline static Reference ParseReference(Engine::Session& session, Engine::RefIdentifier<Reference> identifier)
{
    return Reader::Read(session, session.Decoder().Resolve(identifier));
}

MethodReference Reader::Read(Engine::Session& session, Engine::RefIdentifier<MethodReference> identifier)
{
    return ParseReference(session, identifier);
}

FieldReference Reader::Read(Engine::Session& session, Engine::RefIdentifier<FieldReference> identifier)
{
    return ParseReference(session, identifier);
}

} // namespace Symlevel
