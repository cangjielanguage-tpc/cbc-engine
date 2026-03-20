#pragma once

#include "engine/engine.h"
#include "offset.h"
#include "string.h"
#include <optional>
#include <vector>

namespace Symlevel {

class MemberIndex;

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

class TypeIndex final {
public:
    static TypeIndex Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t typeIndexOffset);

    TypeIndex(std::unique_ptr<MemberIndex> index);
    TypeIndex(TypeIndex&&);
    ~TypeIndex();

    std::optional<TypeDefinition> FindType(Engine::Session& session, String typeName) const;

private:
    std::unique_ptr<MemberIndex> index;
};

class FieldIndex final {
public:
    static FieldIndex Read(IO::StreamFileReader& reader, IO::FileId fileId);

    FieldIndex(std::unique_ptr<MemberIndex> index);
    FieldIndex(FieldIndex&&);
    ~FieldIndex();

    std::optional<FieldDefinition> FindField(Engine::Session& session, String fieldName) const;

private:
    std::unique_ptr<MemberIndex> index;
};

class MethodIndex final {
public:
    static MethodIndex Read(IO::StreamFileReader& reader, IO::FileId fileId);

    MethodIndex(std::unique_ptr<MemberIndex>);
    MethodIndex(MethodIndex&&);
    ~MethodIndex();

    std::vector<MethodDefinition> FindMethods(Engine::Session& session, String methodName) const;

private:
    std::unique_ptr<MemberIndex> index;
};

} // namespace Symlevel
