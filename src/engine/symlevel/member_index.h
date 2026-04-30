#pragma once

#include "engine/engine.h"
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace Symlevel {

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

struct MemberIndex {
    IO::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    static MemberIndex Read(IO::FileId fileId, IO::StreamFileReader& reader);
};

template <typename Index> class MemberIndexBase {
public:
    MemberIndex index;

    static Index Read(IO::StreamFileReader& reader, IO::FileId fileId)
    {
        return Index { MemberIndex::Read(fileId, reader) };
    }
};

class TypeIndex : public MemberIndexBase<TypeIndex> {
public:
    std::optional<Engine::Identifier<TypeDefinition>> FindType(
        Engine::Session& session, std::string_view typeName
    ) const;

    void ForEach(Engine::Session& session, std::function<void(TypeDefinition&)> action) const;
};

class FieldIndex : public MemberIndexBase<FieldIndex> {
public:
    std::optional<Engine::Identifier<FieldDefinition>> FindField(
        Engine::Session& session, std::string_view fieldName
    ) const;

    void Find(Engine::Session& session, std::function<bool(FieldDefinition&)> action) const;
    void ForEach(Engine::Session& session, std::function<void(FieldDefinition&)> action) const;
};

class MethodIndex : public MemberIndexBase<MethodIndex> {
public:
    std::vector<Engine::Identifier<MethodDefinition>> FindMethods(
        Engine::Session& session, std::string_view methodName
    ) const;

    void ForEach(Engine::Session& session, std::function<void(MethodDefinition&)> action) const;
};

} // namespace Symlevel
