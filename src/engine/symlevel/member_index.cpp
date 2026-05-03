#include <algorithm>

#include "definitions.h"
#include "io/offset_pool.h"
#include "io/stream_file_reader.h"
#include "member_index.h"
#include "reader.h"

namespace Symlevel {

/**
 * @brief Bucket-based hash table of file entities which can be accessed by enitity name.
 *
 * Let's say we have @c memberCount members in index which can be split to @c bucketCount buckets
 * by some hash-function. We store @c bucketTable, each element of which is @c start index
 * of corresponding bucket. The @c buckets themselves are ordered by indicies in the table
 * and stored flat in a single array.
 *
 * Note that if @c bucketTable(i) is @c start index of bucket then @c bucketTable(i+1)
 * is @c end index of the same bucket exclusively.
 *
 * For completness, we extend bucket table with additional element which holds @c buckets length.
 *
 * Examples:
 * @code
 * 1.
 *      bucketTable: [0,  0,  1,  3,  4,  4,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = ()
 *      bucket1 = (x0)
 *      bucket2 = (x1, x2)
 *      bucket3 = (x3)
 *      bucket4 = ()
 *      bucket5 = (x4, x5)
 *
 * 2.
 *      bucketTable: [0,  2,  2,  2,  3,  6,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = (x0, x1)
 *      bucket1 = ()
 *      bucket2 = ()
 *      bucket3 = (x2)
 *      bucket4 = (x3, x4, x5)
 *      bucket5 = ()
 *
 * 3.
 *      bucketTable: [0,  1,  2,  3,  4,  5,  6]
 *      buckets:     [x0, x1, x2, x3, x4, x5]
 *
 *      bucket0 = (x0)
 *      bucket1 = (x1)
 *      bucket2 = (x2)
 *      bucket3 = (x3)
 *      bucket4 = (x4)
 *      bucket5 = (x5)
 * @endcode
 *
 * Note that @c buckets length is @c memberCount
 * Note that @c bucketTable length is @c bucketCount+1
 */
class MemberIndex final {
public:
    IO::FileId fileId;
    uint32_t bucketCount;
    uint32_t memberCount;
    // IO::OffsetPool bucketTable;
    // IO::OffsetPool buckets;

    static std::unique_ptr<MemberIndex> Read(IO::StreamFileReader& reader, IO::FileId fileId)
    {
        auto bucketTableSize = reader.ReadU32();
        auto bucketsSize     = reader.ReadU32();

        auto bucketTableOffs = reader.Position();
        auto bucketsOffs     = bucketTableOffs + bucketTableSize * sizeof(uint32_t);

        // IO::OffsetPool bucketTable(bucketTableOffs, bucketTableSize);
        // IO::OffsetPool buckets(bucketsOffs, bucketsSize);

        reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));

        return std::make_unique<MemberIndex>(fileId, bucketTableSize - 1, bucketsSize);
    }

    MemberIndex(
        IO::FileId fileId,
        uint32_t bucketCount,
        uint32_t memberCount
    )
        : fileId(fileId),
          bucketCount(bucketCount),
          memberCount(memberCount)
    {}

    static uint32_t Hash(String name)
    {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
        uint32_t hash       = 0;
        for (size_t i = 0; i < name.size(); i++) {
            hash = (hash << 5) - hash + (data[i] & 0xFF);
        }
        return hash;
    }

    bool IsEmpty() const { return memberCount == 0; }

    template <typename T> std::optional<Offset<T>> FindOffset(Engine::Session& session, String name) const
    {
//        if (IsEmpty()) {
//            return std::nullopt;
//        }
//
//        auto& file = *session.FileOf(fileId);
//
//        uint32_t startIdx = Hash(name) % bucketCount;
//
//        // TODO: the same as aot table!
//        auto start = bucketTable.QueryOffset(file, startIdx);
//        auto end   = bucketTable.QueryOffset(file, startIdx + 1);
//        ASSERT(start <= end);
//
//        for (auto i = start; i < end; i++) {
//            auto entityOffs = Offset<T>(buckets.QueryOffset(file, i));
//            auto entityName = Reader::ReadName(session, fileId, entityOffs);
//            if (entityName.compare(name) == 0) {
//                return entityOffs;
//            }
//        }
//
        return std::nullopt;
    }

    template <typename T> std::vector<Offset<T>> FindOffsets(Engine::Session& session, String name) const
    {
//        if (IsEmpty()) {
//            return {};
//        }
//
//        auto& file = *session.FileOf(fileId);
//
//        uint32_t startIdx = Hash(name) % bucketCount;
//
//        auto start = bucketTable.QueryOffset(file, startIdx);
//        auto end   = bucketTable.QueryOffset(file, startIdx + 1);
//        ASSERT(start <= end);
//
//        std::vector<Offset<T>> offsets;
//        for (auto i = start; i < end; i++) {
//            auto entityOffs = Offset<T>(buckets.QueryOffset(file, i));
//            auto entityName = Reader::ReadName(session, fileId, entityOffs);
//            if (entityName.compare(name) == 0) {
//                offsets.push_back(entityOffs);
//            }
//        }
//
        return std::vector<Offset<T>>();
    }
};

TypeIndex::TypeIndex(std::unique_ptr<MemberIndex> index) : index(std::move(index)) {}

TypeIndex::TypeIndex(TypeIndex&&) = default;
TypeIndex::~TypeIndex()           = default;

TypeIndex TypeIndex::Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t typeIndexOffset)
{
    IO::StreamFileReader reader(file, typeIndexOffset);
    return TypeIndex(MemberIndex::Read(reader, fileId));
}

std::optional<TypeDefinition> TypeIndex::FindType(Engine::Session& session, String typeName) const
{
    auto offset = index->FindOffset<TypeDefinition>(session, typeName);

    if (offset) {
        return Reader::Read(session, index->fileId, *offset);
    } else {
        return std::nullopt;
    }
}

FieldIndex::FieldIndex(std::unique_ptr<MemberIndex> index) : index(std::move(index)) {}

FieldIndex::FieldIndex(FieldIndex&&) = default;
FieldIndex::~FieldIndex()            = default;

FieldIndex FieldIndex::Read(IO::StreamFileReader& reader, IO::FileId fileId)
{
    return FieldIndex(MemberIndex::Read(reader, fileId));
}

std::optional<FieldDefinition> FieldIndex::FindField(Engine::Session& session, String fieldName) const
{
    auto offset = index->FindOffset<FieldDefinition>(session, fieldName);

    if (offset) {
        return Reader::Read(session, index->fileId, *offset);
    } else {
        return std::nullopt;
    }
}

MethodIndex::MethodIndex(std::unique_ptr<MemberIndex> index) : index(std::move(index)) {}

MethodIndex::MethodIndex(MethodIndex&&) = default;
MethodIndex::~MethodIndex()             = default;

MethodIndex MethodIndex::Read(IO::StreamFileReader& reader, IO::FileId fileId)
{
    return MethodIndex(MemberIndex::Read(reader, fileId));
}

std::vector<Engine::Identifier<MethodDefinition>> MethodIndex::FindMethods(Engine::Session& session, String methodName) const
{
    auto offsets = index->FindOffsets<MethodDefinition>(session, methodName);

    if (offsets.empty()) {
        return {};
    } else {
        std::vector<Engine::Identifier<MethodDefinition>> defs;
        for (auto& offset : offsets) {
            defs.push_back(Engine::Identifier<MethodDefinition>(offset, index->fileId));
        }
        return defs;
    }
}

} // namespace Symlevel
