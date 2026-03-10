#pragma once

#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"

namespace Symlevel {

// defs
class TypeDefinition;
class MethodDefinition;
class FieldDefinition;
class TermVal;

// refs
class MethodReference;
class FieldReference;

// metadata
class TypeIndex;
class RegionData;

// misc
class Code;
class String;

class CbcFile {
private:
    struct Impl;
    friend struct Impl;

public:
    static CbcFile Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name);

    CbcFile(std::unique_ptr<Impl> impl);
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
    String GetName() const;

    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
