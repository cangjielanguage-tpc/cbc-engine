#include "references.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include "region_data.h"

namespace Symlevel {

std::optional<MethodReference> MethodReference::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto refTypeIdx   = reader.ReadULEB();
    auto methodSigIdx = reader.ReadULEB();

    auto specialFlags = reader.ReadU8();  // TODO: remove
    auto accessKind   = reader.ReadU16(); // TODO: remove

    auto name = Reader::Read(session, fileId, nameOffset);

    auto regionData = session.CbcFileOf(fileId).GetRegionData();

    auto refType   = regionData.queryTerm(session, { .region = 0, .index = refTypeIdx });
    auto methodSig = regionData.queryTerm(session, { .region = 0, .index = methodSigIdx });

    if (refType.has_value() && methodSig.has_value()) {
        return MethodReference(name, refType.value(), methodSig.value());
    } else {
        return std::nullopt;
    }
}

std::optional<FieldReference> FieldReference::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + offset);

    auto nameOffset   = Offset<String>(reader.ReadU32());
    auto refTypeIdx   = reader.ReadULEB();
    auto fieldTypeIdx = reader.ReadULEB();

    auto accessKind = reader.ReadU16(); // TODO: remove

    auto name = Reader::Read(session, fileId, nameOffset);

    auto regionData = session.CbcFileOf(fileId).GetRegionData();

    auto refType   = regionData.queryTerm(session, Index<Terms::Term> { .region = 0, .index = refTypeIdx });
    auto fieldType = regionData.queryTerm(session, Index<Terms::Term> { .region = 0, .index = fieldTypeIdx });

    if (refType.has_value() && fieldType.has_value()) {
        return FieldReference(name, refType.value(), fieldType.value());
    } else {
        return std::nullopt;
    }
}

} // namespace Symlevel
