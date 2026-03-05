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
    uint32_t GetCodeOffs(Offset<Code> offs) const;
    uint32_t GetStringOffs(Offset<String> offs) const;
    uint32_t GetTypeDefOffs(Offset<TypeDefinition> offs) const;
    uint32_t GetMethodDefOffs(Offset<MethodDefinition> offs) const;
    uint32_t GetFieldDefOffs(Offset<FieldDefinition> offs) const;
    uint32_t GetTermOffs(Offset<TermVal> offs) const;
    uint32_t GetMethodRefOffset(Offset<MethodReference> offs) const;
    String GetName() const;

    const RegionData& GetRegionData() const;
    const TypeIndex& GetTypeIndex() const;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
