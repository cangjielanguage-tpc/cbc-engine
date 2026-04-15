#include "region_data.h"
#include "io/stream_file_reader.h"
#include "reader.h"

namespace Symlevel {

RegionData RegionData::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    uint16_t typeIdxSize = reader.ReadU16(); // TODO: remove
    uint32_t typeIdxOffs = reader.ReadU32(); // TODO: remove

    uint16_t methodIndexSize = reader.ReadU16();
    uint32_t methodIndexOffs = reader.ReadU32();

    uint16_t fieldIndexSize = reader.ReadU16();
    uint32_t fieldIndexOffs = reader.ReadU32();

    uint32_t termIndexSize = reader.ReadULEB();
    uint32_t termIndexOffs = reader.ReadU32();

    IO::OffsetPool methods(methodIndexOffs, methodIndexSize);
    IO::OffsetPool fields(fieldIndexOffs, fieldIndexSize);
    IO::OffsetPool terms(termIndexOffs, termIndexSize, FirstNonBuiltIn());

    return RegionData(fileId, methods, fields, terms);
}

RegionData::RegionData(IO::FileId fileId, IO::OffsetPool methods, IO::OffsetPool fields, IO::OffsetPool terms)
    : fileId(fileId),
      methods(methods),
      fields(fields),
      terms(terms)
{}

std::optional<MethodReference> RegionData::queryMethod(Engine::Session& session, Index<MethodReference> index) const
{
    auto offs = methods.QueryOffset(*session.FileOf(fileId), index);
    return Reader::ReadAndResolve(session, fileId, offs);
}

std::optional<Term> RegionData::queryTerm(Engine::Session& session, Index<Term> index) const
{
    if (IsBuiltin(index.index)) {
        return Term::Builtin(session, TemplateKind(index.index));
    } else {
        auto offs = terms.QueryOffset(*session.FileOf(fileId), index);
        return Reader::ReadAndResolve(session, fileId, offs);
    }
}

} // namespace Symlevel
