#include "cbc_file.h"

#include "aot_table.h"
#include "io/stream_file_reader.h"
#include "member_index.h"
#include "region_data.h"
#include "version_metadata.h"

namespace Symlevel {

struct CbcFile::Impl {
    VersionMetadata versionMetadata;
    TypeIndex typeIndex;
    RegionData regionData;

    DirectCallAotTable directCallAotTable;
    VirtualCallAotTable virtualCallAotTable;
    InterfaceCallAotTable interfaceCallAotTable;
    StaticFieldAotTable staticFieldAotTable;
    InstanceFieldAotTable instanceFieldAotTable;

    uint32_t poolOffset;
    IO::FileId id;
    std::string name;
};

CbcFile::CbcFile(std::unique_ptr<CbcFile::Impl> impl) : impl(std::move(impl)) {}

CbcFile::CbcFile(CbcFile&& other) = default;
CbcFile::~CbcFile()               = default;

CbcFile CbcFile::Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name)
{
    IO::StreamFileReader reader(file, 0);

    static const uint32_t MAGIC              = 0x00434243; // 'C', 'B', 'C' \0
    static const uint32_t MAGIC_MASK         = 0x00FFFFFF;
    static const uint32_t FILE_VERSION_SHIFT = 24;

    auto magicAndVersion = reader.ReadU32();

    auto magic = magicAndVersion & MAGIC_MASK;
    if (magic != MAGIC) {
        // TODO: throw proper exception
        ASSERTION(false, "invalid magic");
    }

    auto fileVersion     = static_cast<uint8_t>(magicAndVersion >> FILE_VERSION_SHIFT);
    auto bytecodeVersion = reader.ReadU8();
    VersionMetadata versionMetadata(fileVersion, bytecodeVersion);

    auto fileProperties = reader.ReadU8();

    auto typeIndexOffset = reader.ReadU32();
    auto poolOffset      = reader.ReadU32();

    auto directCallAotTableOffset    = reader.ReadU32();
    auto virtualCallAotTableOffset   = reader.ReadU32();
    auto interfaceCallAotTableOffset = reader.ReadU32();
    auto staticFieldAotTableOffset   = reader.ReadU32();
    auto instanceFieldAotTableOffset = reader.ReadU32();

    auto regionNum = reader.ReadU16();
    if (regionNum != 1) {
        // TODO: throw proper exception
        ASSERTION(false, "unsupported region num");
    }
    auto regionOffset = reader.ReadU32();

    auto mainType    = reader.ReadU32();
    auto foreignLibs = reader.ReadU32();
    auto coverageId  = reader.ReadULEB();

    CbcFile::Impl impl {
        .versionMetadata       = versionMetadata,
        .typeIndex             = TypeIndex::Read(fileId, file, typeIndexOffset),
        .regionData            = RegionData::Read(fileId, file, regionOffset),
        .directCallAotTable    = DirectCallAotTable::Read(fileId, file, directCallAotTableOffset),
        .virtualCallAotTable   = VirtualCallAotTable::Read(fileId, file, virtualCallAotTableOffset),
        .interfaceCallAotTable = InterfaceCallAotTable::Read(fileId, file, interfaceCallAotTableOffset),
        .staticFieldAotTable   = StaticFieldAotTable::Read(fileId, file, staticFieldAotTableOffset),
        .instanceFieldAotTable = InstanceFieldAotTable::Read(fileId, file, instanceFieldAotTableOffset),
        .poolOffset            = poolOffset,
        .id                    = fileId,
        .name                  = std::string(name),
    };

    return CbcFile(std::make_unique<CbcFile::Impl>(std::move(impl)));
}

IO::FileId CbcFile::Id() const { return impl->id; }

uint32_t CbcFile::GetCodeSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetStringSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetTypeDefSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetMethodDefSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetFieldDefSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetTermSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetMethodRefSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetFieldRefSectionOffs() const { return impl->poolOffset; }

uint32_t CbcFile::GetAotDataSectionOffs() const { return impl->poolOffset; }

String CbcFile::GetName() const { return String(impl->name); }

// FIXME:store path and name of cbc file.
String CbcFile::GetPath() const { return String(impl->name); }

const RegionData& CbcFile::GetRegionData() const { return impl->regionData; }

const TypeIndex& CbcFile::GetTypeIndex() const { return impl->typeIndex; }

const DirectCallAotTable& CbcFile::GetDirectCallAotTable() const { return impl->directCallAotTable; }

const VirtualCallAotTable& CbcFile::GetVirtualCallAotTable() const { return impl->virtualCallAotTable; }

const InterfaceCallAotTable& CbcFile::GetInterfaceCallAotTable() const { return impl->interfaceCallAotTable; }

const StaticFieldAotTable& CbcFile::GetStaticFieldAotTable() const { return impl->staticFieldAotTable; }

const InstanceFieldAotTable& CbcFile::GetInstanceFieldAotTable() const { return impl->instanceFieldAotTable; }

} // namespace Symlevel
