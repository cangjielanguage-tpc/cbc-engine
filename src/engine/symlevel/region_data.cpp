#include "region_data.h"
#include "engine/symlevel/term.h"
#include "io/stream_file_reader.h"

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

    IO::OffsetPool<MethodReference> methods(methodIndexOffs, methodIndexSize);
    IO::OffsetPool<FieldReference> fields(fieldIndexOffs, fieldIndexSize);
    IO::OffsetPool<Term> terms(termIndexOffs, termIndexSize);

    return RegionData(fileId, methods, fields, terms);
}

RegionData::RegionData(
    IO::FileId fileId,
    IO::OffsetPool<MethodReference> methods,
    IO::OffsetPool<FieldReference> fields,
    IO::OffsetPool<Term> terms
)
    : fileId(fileId),
      methods(methods),
      fields(fields),
      terms(terms)
{}

template <typename T> using OffsetId = Engine::Identifier<T>;

Offset<MethodReference> RegionData::Query(Engine::Session& session, Index<MethodReference> index) const
{
    // FIXME: use region idx
    return methods.QueryOffset(*session.FileOf(fileId), index.GetIndex());
}

Offset<FieldReference> RegionData::Query(Engine::Session& session, Index<FieldReference> index) const
{
    // FIXME: use region idx
    return fields.QueryOffset(*session.FileOf(fileId), index.GetIndex());
}

Offset<Term> RegionData::Query(Engine::Session& session, Index<Term> index) const
{
    // FIXME: use region idx
    // adjust index by the number of primitive types.
    return terms.QueryOffset(*session.FileOf(fileId), index.GetIndex() - Term::FIRST_NON_PRIMITIVE);
}

} // namespace Symlevel
