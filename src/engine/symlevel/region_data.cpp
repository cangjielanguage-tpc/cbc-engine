#include "engine/terms.h"
#include "io/stream_file_reader.h"

namespace Symlevel {

static_assert(RegionData::FIRST_NON_PRIMITIVE_TERM_ID == Engine::FIRST_NON_PRIMITIVE);

RegionData RegionData::Read(Symlevel::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
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

RegionData::RegionData(
    OffsetPool<MethodReference> methods,
    OffsetPool<FieldReference> fields,
    OffsetPool<Term, Engine::FIRST_NON_PRIMITIVE> terms
)
    : methods(methods),
      fields(fields),
      terms(terms)
{}

} // namespace Symlevel
