#pragma once

#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"

namespace Symlevel {

// FIXME: - remove explict cbc file header usages.
//        - provide implicit cbc file usages via file id.

// defs
class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

// refs
class MethodReference;
class FieldReference;

// metadata
class TypeIndex;
class RegionData;
class Dependencies;

// aot tables
class DirectCallAotTable;
class VirtualCallAotTable;
class InterfaceCallAotTable;
class StaticFieldAotTable;
class InstanceFieldAotTable;

// misc
class Code;
class String;

class CbcFile {
public:
    static CbcFile Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name);

    CbcFile(CbcFile&& other);
    ~CbcFile();

    IO::FileId Id() const;
    uint32_t GetCodeSectionOffs() const;
    uint32_t GetStringSectionOffs() const;
    uint32_t GetTypeDefSectionOffs() const;
    uint32_t GetMethodDefSectionOffs() const;
    uint32_t GetFieldDefSectionOffs() const;
    uint32_t GetTermSectionOffs() const;
    uint32_t GetMethodRefSectionOffs() const;
    uint32_t GetFieldRefSectionOffs() const;
    uint32_t GetAotDataSectionOffs() const;

    String GetName() const;
    String GetPath() const;

    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;
    const Dependencies& GetDependencies() const;

    const DirectCallAotTable& GetDirectCallAotTable() const;
    const VirtualCallAotTable& GetVirtualCallAotTable() const;
    const InterfaceCallAotTable& GetInterfaceCallAotTable() const;
    const StaticFieldAotTable& GetStaticFieldAotTable() const;
    const InstanceFieldAotTable& GetInstanceFieldAotTable() const;

private:
    struct Impl;
    friend struct Impl;

    CbcFile(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
