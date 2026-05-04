#include "aot_table.h"
#include "engine/engine.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "utils/assertion.h"
#include <cstdint>

namespace Symlevel {

AotTable AotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    IO::StreamFileReader reader(file, offset);

    auto bucketTableSize = reader.ReadU32();
    auto bucketsSize     = reader.ReadU32();

    auto bucketTableOffs = reader.Position();
    uint32_t bucketsOffs = bucketTableOffs + bucketTableSize * sizeof(uint32_t);

    reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));
    return { fileId, bucketTableOffs, bucketTableSize, bucketsOffs, bucketsSize };
}

template <typename Data> struct AotTableWrapper {
    AotTable const& table;

    uint32_t MemberCount() { return table.bucketsSize; }

    uint32_t BucketCount() { return table.bucketTableSize - 1; }

    bool IsEmpty() { return MemberCount() == 0; }

    static uint32_t Hash(uint32_t idx) { return idx; }

    /// Serialized hash table.
    /// Buckets are stored in continuous array.
    /// Bucket table stores indicies where buckets are starting (with extra slot that points to end of array).
    ///
    /// [b_0, b_1, b_2, .., b_n, b_n+1] bucket table
    /// b_i - start of the bucket i
    /// b_i+1 - end of the bucket i
    Offset<Data> FindData(Engine::Session& session, uint32_t idx)
    {
        ASSERT(!IsEmpty());
        auto [_, raf] = session.File(table.fileId);

        uint32_t bucketIdx = Hash(idx) % BucketCount();

        uint32_t step        = sizeof(uint32_t);
        auto bucketStartOffs = table.bucketTableStart + idx * step;
        auto bucketEndOffs   = table.bucketTableStart + (idx + 1) * step;

        auto dataStartIdx = ReadAt(raf, bucketStartOffs);
        auto dataEndIdx   = ReadAt(raf, bucketEndOffs);
        ASSERT(dataStartIdx <= dataEndIdx);

        for (auto i = dataStartIdx; i < dataEndIdx; i++) {
            auto dataOffs = Offset<Data>(ReadAt(raf, table.bucketsStart + i * step));
            auto dataIdx  = ReadAt(raf, dataOffs);
            if (idx == dataIdx) {
                return dataOffs;
            }
        }
        FATAL("AOT data was incorrectly encoded");
    }

    uint32_t ReadAt(IO::RandomAccessFile& raf, uint32_t offs) const
    {
        return IO::StreamFileReader(raf, offs).ReadU32();
    }
};

DirectCallAotData DirectCallAotTable::GetData(Engine::Session& session, Index<MethodReference> index) const
{
    AotTableWrapper<DirectCallAotData> wrapper { table };
    auto offs = wrapper.FindData(session, index.Raw());

    auto [file, raf] = session.File(table.fileId);
    IO::StreamFileReader reader(raf, file.GetAotDataSectionOffs() + offs);

    auto _        = reader.ReadU32();
    auto nameOffs = Offset<String>(reader.ReadU32());
    return { Engine::Identifier(nameOffs, table.fileId) };
}

VirtualCallAotData VirtualCallAotTable::GetData(Engine::Session& session, Index<MethodReference> index) const
{
    AotTableWrapper<VirtualCallAotData> wrapper { table };
    auto offs = wrapper.FindData(session, index.Raw());

    auto [file, raf] = session.File(table.fileId);
    IO::StreamFileReader reader(raf, file.GetAotDataSectionOffs() + offs);

    auto _         = reader.ReadU32();
    auto methodNum = reader.ReadU16();
    auto extDefNum = reader.ReadU16();
    return { methodNum, extDefNum };
}

InterfaceCallAotData InterfaceCallAotTable::GetData(Engine::Session& session, Index<MethodReference> index) const
{
    AotTableWrapper<InterfaceCallAotData> wrapper { table };
    auto offs = wrapper.FindData(session, index.Raw());

    auto [file, raf] = session.File(table.fileId);
    IO::StreamFileReader reader(raf, file.GetAotDataSectionOffs() + offs);

    auto _         = reader.ReadU32();
    auto methodNum = reader.ReadU16();
    return { methodNum };
}

StaticFieldAotData StaticFieldAotTable::GetData(Engine::Session& session, Index<FieldReference> index) const
{
    AotTableWrapper<StaticFieldAotData> wrapper { table };
    auto offs = wrapper.FindData(session, index.Raw());

    auto [file, raf] = session.File(table.fileId);
    IO::StreamFileReader reader(raf, file.GetAotDataSectionOffs() + offs);

    auto _        = reader.ReadU32();
    auto nameOffs = Offset<String>(reader.ReadU32());
    return { Engine::Identifier(nameOffs, table.fileId) };
}

InstanceFieldAotData InstanceFieldAotTable::GetData(Engine::Session& session, Index<FieldReference> index) const
{
    AotTableWrapper<StaticFieldAotData> wrapper { table };
    auto offs = wrapper.FindData(session, index.Raw());

    auto [file, raf] = session.File(table.fileId);
    IO::StreamFileReader reader(raf, file.GetAotDataSectionOffs() + offs);

    auto _       = reader.ReadU32();
    auto ordinal = reader.ReadU32();
    return { ordinal };
}

} // namespace Symlevel
