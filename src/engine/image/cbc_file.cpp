#include "cbc_file.h"

#include "engine/decode/decoder.h"
#include "engine/decode/reader.h"
#include "io/stream_file_reader.h"
#include "version_metadata.h"
#include <optional>

namespace Image {

Image::FileId CbcFile::Id() const { return this->id; }

uint32_t CbcFile::GetCodeSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetStringSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetTypeDefSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetMethodDefSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetFieldDefSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetTermSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetMethodRefSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetFieldRefSectionOffs() const { return this->poolOffset; }

uint32_t CbcFile::GetAotDataSectionOffs() const { return this->poolOffset; }

// FIXME:store path and name of cbc file.
String CbcFile::GetPath() const { return String(this->name); }

const RegionData& CbcFile::GetRegionData() const { return this->regionData; }

const TypeIndex& CbcFile::GetTypeIndex() const { return this->typeIndex; }

std::optional<Offset<String>> CbcFile::AotDependencies() const
{
    if (this->aotDeps < 0) {
        return std::nullopt;
    }
    return Offset<String>(this->aotDeps);
}

std::optional<Offset<String>> CbcFile::CbcDependencies() const
{
    if (this->cbcDeps < 0) {
        return std::nullopt;
    }
    return Offset<String>(this->cbcDeps);
}

const std::optional<Identifier<String>> CbcFile::GetMainTypeName() const { return this->mainTypeName; }

const DirectCallAotTable& CbcFile::GetDirectCallAotTable() const { return this->directCallAotTable; }

const VirtualCallAotTable& CbcFile::GetVirtualCallAotTable() const { return this->virtualCallAotTable; }

const InterfaceCallAotTable& CbcFile::GetInterfaceCallAotTable() const { return this->interfaceCallAotTable; }

const StaticFieldAotTable& CbcFile::GetStaticFieldAotTable() const { return this->staticFieldAotTable; }

const InstanceFieldAotTable& CbcFile::GetInstanceFieldAotTable() const { return this->instanceFieldAotTable; }

} // namespace Image
