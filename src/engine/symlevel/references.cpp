#include "references.h"
#include "io/stream_file_reader.h"
#include "reader.h"

namespace Symlevel {

MethodReference MethodReference::Parse(Engine::Session& session, IO::FileId fileId, Offset<MethodReference> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetMethodRefSectionOffs() + offset);

    auto nameOffset   = reader.ReadU32();
    auto refTypeIdx   = reader.ReadULEB();
    auto methodSigIdx = reader.ReadULEB();

    auto specialFlags = reader.ReadU8();  // TODO: remove
    auto accessKind   = reader.ReadU16(); // TODO: remove

    auto name = Reader::Read(session, fileId, Offset<String>(nameOffset));
    Term refType;   // TODO: impl
    Term methodSig; // TODO: impl

    return MethodReference(name, refType, methodSig);
}

FieldReference FieldReference::Parse(Engine::Session& session, IO::FileId fileId, Offset<FieldReference> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetFieldRefSectionOffs() + offset);

    auto nameOffset   = reader.ReadU32();
    auto refTypeIdx   = reader.ReadULEB();
    auto fieldTypeIdx = reader.ReadULEB();

    auto accessKind = reader.ReadU16(); // TODO: remove

    auto name = Reader::Read(session, fileId, Offset<String>(nameOffset));
    Term refType;   // TODO: impl
    Term fieldType; // TODO: impl

    return FieldReference(name, refType, fieldType);
}

} // namespace Symlevel
