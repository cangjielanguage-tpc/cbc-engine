#include "aot_table.h"
#include "io/offset_pool.h"
#include "reader.h"

namespace Symlevel {

class AotTable {
public:
    IO::FileId fileId;
    uint32_t bucketCount;
    uint32_t memberCount;
    IO::OffsetPool bucketTable;
    IO::OffsetPool buckets;

    static std::unique_ptr<AotTable> Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
    {
        IO::StreamFileReader reader(file, offset);

        auto bucketTableSize = reader.ReadU32();
        auto bucketsSize     = reader.ReadU32();

        auto bucketTableOffs = reader.Position();
        auto bucketsOffs     = bucketTableOffs + bucketTableSize * sizeof(uint32_t);

        IO::OffsetPool bucketTable(bucketTableOffs, bucketTableSize);
        IO::OffsetPool buckets(bucketsOffs, bucketsSize);

        reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));

        return std::make_unique<AotTable>(fileId, bucketTableSize - 1, bucketsSize, bucketTable, buckets);
    }

    AotTable(
        IO::FileId fileId,
        uint32_t bucketCount,
        uint32_t memberCount,
        IO::OffsetPool bucketTable,
        IO::OffsetPool buckets
    )
        : fileId(fileId),
          bucketCount(bucketCount),
          memberCount(memberCount),
          bucketTable(bucketTable),
          buckets(buckets)
    {}

    template <typename Ref> static uint32_t Hash(Index<Ref> index) { return index.index; }

    bool IsEmpty() const { return memberCount == 0; }

    template <typename Ref, typename Data>
    std::optional<Offset<Data>> FindData(Engine::Session& session, Index<Ref> idx)
    {
        if (IsEmpty()) {
            return std::nullopt;
        }

        auto& file = *session.FileOf(fileId);

        uint32_t startIdx = Hash(idx) % bucketCount;

        auto start = bucketTable.QueryOffset(file, startIdx);
        auto end   = bucketTable.QueryOffset(file, startIdx + 1);
        ASSERT(start <= end);

        for (auto i = start; i < end; i++) {
            auto dataOffs = Offset<Data>(buckets.QueryOffset(file, i));
            auto dataIdx  = Reader::ReadIndex<Ref, Data>(session, fileId, dataOffs);
            if (idx.raw == dataIdx.raw) {
                return dataOffs;
            }
        }
        return std::nullopt;
    }
};

///////////////////////////////
// DirectCall

Index<MethodReference> DirectCallAotData::ParseIndex(
    Engine::Session& session, IO::FileId fileId, Offset<DirectCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);
    auto index = reader.ReadU32();
    return { .region = 0, .index = index };
}

DirectCallAotData DirectCallAotData::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<DirectCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);

    auto index      = reader.ReadU32();
    auto nameOffset = Offset<String>(reader.ReadU32());

    return DirectCallAotData(Reader::Read(session, fileId, nameOffset));
}

DirectCallAotTable DirectCallAotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    return DirectCallAotTable(AotTable::Read(fileId, file, offset));
}

DirectCallAotTable::DirectCallAotTable(std::unique_ptr<AotTable> aotTable) : aotTable(std::move(aotTable)) {}

DirectCallAotTable::DirectCallAotTable(DirectCallAotTable&& other) = default;
DirectCallAotTable::~DirectCallAotTable()                          = default;

std::optional<DirectCallAotData> DirectCallAotTable::GetData(
    Engine::Session& session, Index<MethodReference> index
) const
{
    auto offset = aotTable->FindData<MethodReference, DirectCallAotData>(session, index);

    if (offset.has_value()) {
        return Reader::ReadAndResolve(session, aotTable->fileId, Offset<DirectCallAotData>(offset.value()));
    } else {
        return std::nullopt;
    }
}

///////////////////////////////
// VirtualCall

Index<MethodReference> VirtualCallAotData::ParseIndex(
    Engine::Session& session, IO::FileId fileId, Offset<VirtualCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);
    auto index = reader.ReadU32();
    return { .region = 0, .index = index };
}

VirtualCallAotData VirtualCallAotData::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<VirtualCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);

    auto index     = reader.ReadU32();
    auto vnum      = reader.ReadU16();
    auto extDefNum = reader.ReadU16();

    return VirtualCallAotData(vnum, extDefNum);
}

VirtualCallAotTable VirtualCallAotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    return VirtualCallAotTable(AotTable::Read(fileId, file, offset));
}

VirtualCallAotTable::VirtualCallAotTable(std::unique_ptr<AotTable> aotTable) : aotTable(std::move(aotTable)) {}

VirtualCallAotTable::VirtualCallAotTable(VirtualCallAotTable&& other) = default;
VirtualCallAotTable::~VirtualCallAotTable()                           = default;

std::optional<VirtualCallAotData> VirtualCallAotTable::GetData(
    Engine::Session& session, Index<MethodReference> index
) const
{
    auto offset = aotTable->FindData<MethodReference, VirtualCallAotData>(session, index);

    if (offset.has_value()) {
        return Reader::ReadAndResolve(session, aotTable->fileId, Offset<VirtualCallAotData>(offset.value()));
    } else {
        return std::nullopt;
    }
}

///////////////////////////////
// IntefaceCall

Index<MethodReference> InterfaceCallAotData::ParseIndex(
    Engine::Session& session, IO::FileId fileId, Offset<InterfaceCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);
    auto index = reader.ReadU32();
    return { .region = 0, .index = index };
}

InterfaceCallAotData InterfaceCallAotData::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<InterfaceCallAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);

    auto index = reader.ReadU32();
    auto inum  = reader.ReadU32();

    return InterfaceCallAotData(inum);
}

InterfaceCallAotTable InterfaceCallAotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    return InterfaceCallAotTable(AotTable::Read(fileId, file, offset));
}

InterfaceCallAotTable::InterfaceCallAotTable(std::unique_ptr<AotTable> aotTable) : aotTable(std::move(aotTable)) {}

InterfaceCallAotTable::InterfaceCallAotTable(InterfaceCallAotTable&& other) = default;
InterfaceCallAotTable::~InterfaceCallAotTable()                             = default;

std::optional<InterfaceCallAotData> InterfaceCallAotTable::GetData(
    Engine::Session& session, Index<MethodReference> index
) const
{
    auto offset = aotTable->FindData<MethodReference, InterfaceCallAotData>(session, index);

    if (offset.has_value()) {
        return Reader::ReadAndResolve(session, aotTable->fileId, Offset<InterfaceCallAotData>(offset.value()));
    } else {
        return std::nullopt;
    }
}

///////////////////////////////
// StaticField

Index<FieldReference> StaticFieldAotData::ParseIndex(
    Engine::Session& session, IO::FileId fileId, Offset<StaticFieldAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);
    auto index = reader.ReadU32();
    return { .region = 0, .index = index };
}

StaticFieldAotData StaticFieldAotData::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<StaticFieldAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);

    auto index      = reader.ReadU32();
    auto nameOffset = Offset<String>(reader.ReadU32());

    return StaticFieldAotData(Reader::Read(session, fileId, nameOffset));
}

StaticFieldAotTable StaticFieldAotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    return StaticFieldAotTable(AotTable::Read(fileId, file, offset));
}

StaticFieldAotTable::StaticFieldAotTable(std::unique_ptr<AotTable> aotTable) : aotTable(std::move(aotTable)) {}

StaticFieldAotTable::StaticFieldAotTable(StaticFieldAotTable&& other) = default;
StaticFieldAotTable::~StaticFieldAotTable()                           = default;

std::optional<StaticFieldAotData> StaticFieldAotTable::GetData(
    Engine::Session& session, Index<FieldReference> index
) const
{
    auto offset = aotTable->FindData<FieldReference, StaticFieldAotData>(session, index);

    if (offset.has_value()) {
        return Reader::ReadAndResolve(session, aotTable->fileId, Offset<StaticFieldAotData>(offset.value()));
    } else {
        return std::nullopt;
    }
}

///////////////////////////////
// InstanceField

Index<FieldReference> InstanceFieldAotData::ParseIndex(
    Engine::Session& session, IO::FileId fileId, Offset<InstanceFieldAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);
    auto index = reader.ReadU32();
    return { .region = 0, .index = index };
}

InstanceFieldAotData InstanceFieldAotData::ParseAndResolve(
    Engine::Session& session, IO::FileId fileId, Offset<InstanceFieldAotData> offset
)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetAotDataSectionOffs() + offset);

    auto index   = reader.ReadU32();
    auto ordinal = reader.ReadU32();

    return InstanceFieldAotData(ordinal);
}

InstanceFieldAotTable InstanceFieldAotTable::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset)
{
    return InstanceFieldAotTable(AotTable::Read(fileId, file, offset));
}

InstanceFieldAotTable::InstanceFieldAotTable(std::unique_ptr<AotTable> aotTable) : aotTable(std::move(aotTable)) {}

InstanceFieldAotTable::InstanceFieldAotTable(InstanceFieldAotTable&& other) = default;
InstanceFieldAotTable::~InstanceFieldAotTable()                             = default;

std::optional<InstanceFieldAotData> InstanceFieldAotTable::GetData(
    Engine::Session& session, Index<FieldReference> index
) const
{
    auto offset = aotTable->FindData<FieldReference, InstanceFieldAotData>(session, index);

    if (offset.has_value()) {
        return Reader::ReadAndResolve(session, aotTable->fileId, Offset<InstanceFieldAotData>(offset.value()));
    } else {
        return std::nullopt;
    }
}

} // namespace Symlevel
