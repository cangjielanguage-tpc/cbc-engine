#include "member_index.h"
#include "definitions.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include <algorithm>

namespace Symlevel {

class MemberIndex final {
public:
    const IO::FileId fileId;
    const uint32_t size;
    const uint32_t offset;

    MemberIndex(IO::FileId fileId, uint32_t size, uint32_t offset) : fileId(fileId), size(size), offset(offset) {}

    bool IsEmpty() const { return size == 0; }

    template <typename T> std::optional<Offset<T>> FindOffset(Engine::Session& session, String name) const
    {
        if (IsEmpty()) {
            return std::nullopt;
        }

        IO::StreamFileReader reader(*session.FileOf(fileId), offset);
        for (uint32_t i = 0; i < size; i++) {
            auto entityOffs = Offset<T>(reader.ReadU32());
            auto entityDef  = Reader::Read(session, fileId, entityOffs);
            auto entityName = Reader::Read(session, fileId, entityDef.Name());
            if (entityName.compare(name) == 0) {
                return entityOffs;
            }
        }

        return std::nullopt;
    }

    template <typename T> std::vector<Offset<T>> FindOffsets(Engine::Session& session, String name) const
    {
        if (IsEmpty()) {
            return {};
        }

        std::vector<Offset<T>> offsets;

        IO::StreamFileReader reader(*session.FileOf(fileId), offset);
        for (uint32_t i = 0; i < size; i++) {
            auto entityOffs = Offset<T>(reader.ReadU32());
            auto entityDef  = Reader::Read(session, fileId, entityOffs);
            auto entityName = Reader::Read(session, fileId, entityDef.Name());
            if (entityName.compare(name) == 0) {
                offsets.push_back(entityOffs);
            }
        }

        return std::move(offsets);
    }
};

TypeIndex::TypeIndex(std::unique_ptr<MemberIndex> index) : index(std::move(index)) {}

TypeIndex::TypeIndex(TypeIndex&&) = default;
TypeIndex::~TypeIndex()           = default;

TypeIndex TypeIndex::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t typeIndexOffset)
{
    IO::StreamFileReader reader(file, typeIndexOffset);

    auto size   = reader.ReadU32();
    auto offset = reader.Position();

    return TypeIndex(std::make_unique<MemberIndex>(fileId, size, offset));
}

std::optional<TypeDefinition> TypeIndex::FindType(Engine::Session& session, String typeName) const
{
    std::optional<Offset<TypeDefinition>> offset = index->FindOffset<TypeDefinition>(session, typeName);

    if (offset) {
        return Reader::Read(session, index->fileId, *offset);
    } else {
        return std::nullopt;
    }
}

MethodIndex::MethodIndex(std::unique_ptr<MemberIndex> index) : index(std::move(index)) {}

MethodIndex::MethodIndex(MethodIndex&&) = default;
MethodIndex::~MethodIndex()             = default;

MethodIndex MethodIndex::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t typeIndexOffset)
{
    IO::StreamFileReader reader(file, typeIndexOffset);

    auto size   = reader.ReadU32();
    auto offset = reader.Position();

    return MethodIndex(std::make_unique<MemberIndex>(fileId, size, offset));
}

std::vector<MethodDefinition> MethodIndex::FindMethods(Engine::Session& session, String methodName) const
{
    std::vector<Offset<MethodDefinition>> offsets = index->FindOffsets<MethodDefinition>(session, methodName);

    if (offsets.empty()) {
        return {};
    } else {
        std::vector<MethodDefinition> defs;
        for (auto& offset : offsets) {
            defs.push_back(Reader::Read(session, index->fileId, offset));
        }
        return defs;
    }
}

} // namespace Symlevel
