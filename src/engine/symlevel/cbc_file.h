#pragma once

#include "code.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include "string.h"

namespace Symlevel {

class MethodDefinition;
class TypeDefinition;
class FieldDefinition;
class MethodReference;
class TermVal;
class MethodReference;
class FieldReference;
class RegionData;
class TypeIndex;

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
    Offset<Code> GetCodeSectionOffs() const;
    Offset<String> GetStringSectionOffs() const;
    Offset<TypeDefinition> GetTypeDefSectionOffs() const;
    Offset<MethodDefinition> GetMethodDefSectionOffs() const;
    Offset<FieldDefinition> GetFieldDefSectionOffs() const;
    Offset<TermVal> GetTermSectionOffs() const;
    Offset<MethodReference> GetMethodRefSectionOffs() const;
    Offset<FieldReference> GetFieldRefSectionOffs() const;
    String GetName() const;

    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
