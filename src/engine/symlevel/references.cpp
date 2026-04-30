#include "references.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include "region_data.h"

namespace Symlevel {

MethodReference MethodReference::Parse(
    Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto refTypeIdx   = Index<Term>(reader.ReadULEB());
    auto methodSigIdx = Index<Term>(reader.ReadULEB());

    auto specialFlags = reader.ReadU8();                    // TODO: remove
    auto accessKind   = reader.ReadU16(); // TODO: remove

    auto name = Reader::Read(session, fileId, nameOffset);

    auto regionData = session.CbcFileOf(fileId).GetRegionData();

    return MethodReference(fileId, nameOffset, refTypeIdx, methodSigIdx);
}

FieldReference FieldReference::Parse(
    Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto refTypeIdx   = Index<Term>(reader.ReadULEB());
    auto fieldTypeIdx = Index<Term>(reader.ReadULEB());

    auto isRecord = static_cast<bool>(reader.ReadU8());

    auto name = Reader::Read(session, fileId, nameOffset);

    auto regionData = session.CbcFileOf(fileId).GetRegionData();

    return FieldReference(fileId, nameOffset, refTypeIdx, fieldTypeIdx, isRecord);
}

} // namespace Symlevel
