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

    return RegionData(
        fileId, methodIndexSize, methodIndexOffs, fieldIndexSize, fieldIndexOffs, termIndexSize, termIndexOffs
    );
}

RegionData::RegionData(
    IO::FileId fileId,
    uint16_t methodIndexSize,
    uint32_t methodIndexOffset,
    uint16_t fieldIndexSize,
    uint32_t fieldIndexOffset,
    uint32_t termIndexSize,
    uint32_t termIndexOffset
)
    : fileId(fileId),
      methodIndexSize(methodIndexSize),
      methodIndexOffset(methodIndexOffset),
      fieldIndexSize(fieldIndexSize),
      fieldIndexOffset(fieldIndexOffset),
      termIndexSize(termIndexSize),
      termIndexOffset(termIndexOffset)
{}

MethodReference RegionData::queryMethod(Engine::Session& session, Index<MethodReference> index) const
{
    ASSERTION(index.index < methodIndexSize, "index is out of range");

    auto& raf = session.FileOf(fileId);
    auto offs = static_cast<uint32_t>(methodIndexOffset + index.index * sizeof(uint32_t));
    IO::StreamFileReader reader(*raf, offs);

    auto sectionOffset = session.CbcFileOf(fileId).GetMethodRefSectionOffs();
    auto refOffset     = reader.ReadU32();

    return Reader::Read(session, fileId, sectionOffset + refOffset);
}

} // namespace Symlevel
