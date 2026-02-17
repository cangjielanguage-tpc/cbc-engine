#pragma once

#include <vector>
#include "io/random_access_file.h"
#include "io/file_id.h"
#include "code.h"
#include "offset.h"

namespace Symlevel {

class MethodDefinition;
class TypeDefinition;
class FieldDefinition;
class MethodReference;
class TermVal;

class CbcFile {
private:
    struct Impl;
    friend struct Impl;

public:
    static constexpr uint32_t MAGIC = 0xCBCDAFF0;

    static CbcFile Create(IO::FileId fileId, IO::RandomAccessFile& file, std::string_view name);

    CbcFile(std::unique_ptr<Impl> impl);
    CbcFile(CbcFile&& other);
    ~CbcFile();

    IO::FileId Id() const;
    Offset<Code> GetCodeSectionOffs() const;
    Offset<TypeDefinition> GetTypeDefSectionOffs() const;
    Offset<MethodDefinition> GetMethodDefSectionOffs() const;
    Offset<FieldDefinition> GetFieldDefSectionOffs() const;
    Offset<TermVal> GetTermSectionOffs() const;

private:
    std::unique_ptr<Impl> impl;
};

} // namespace Symlevel
