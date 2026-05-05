#include "member_index.h"
#include "definitions.h"
#include "engine/identifiers.h"
#include "engine/symlevel/io/random_access_file.h"
#include "io/stream_file_reader.h"
#include "reader.h"
#include <cstdint>
#include <string_view>

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
MemberIndex MemberIndex::Read(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto bucketTableSize = reader.ReadU32();
    auto bucketsSize     = reader.ReadU32();

    auto bucketTableOffs = reader.Position();
    uint32_t bucketsOffs = bucketTableOffs + bucketTableSize * sizeof(uint32_t);

    reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));
    return { fileId, bucketTableOffs, bucketTableSize, bucketsOffs, bucketsSize };
}

template <typename Data> struct MemberIndexWrapper {
    using Identifier = Engine::Identifier<Data>;

    MemberIndex const& index;

    uint32_t MemberCount() const { return index.bucketsSize; }

    uint32_t BucketCount() const { return index.bucketTableSize - 1; }

    bool IsEmpty() const { return MemberCount() == 0; }

    static uint32_t Hash(String name)
    {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
        uint32_t hash       = 0;
        for (size_t i = 0; i < name.size(); i++) {
            hash = (hash << 5) - hash + (data[i] & 0xFF);
        }
        return hash;
    }

    std::optional<Identifier> FindOffset(Engine::Session& session, String name) const
    {
        if (IsEmpty()) {
            return std::nullopt;
        }

        auto [_, raf] = session.File(index.fileId);

        uint32_t startIdx = Hash(name) % BucketCount();
        uint32_t step     = sizeof(uint32_t);

        auto bucketStartOffs = index.bucketTableStart + startIdx * step;
        auto bucketEndOffs   = index.bucketTableStart + (startIdx + 1) * step;

        auto dataStartIdx = ReadAt(raf, bucketStartOffs);
        auto dataEndIdx   = ReadAt(raf, bucketEndOffs);

        ASSERT(dataStartIdx <= dataEndIdx);

        for (auto i = dataStartIdx; i < dataEndIdx; i++) {
            auto dataOffs   = Offset<Data>(ReadAt(raf, index.bucketsStart + i * step));
            auto entityName = Reader::ReadName(session, index.fileId, dataOffs);

            if (entityName.compare(name) == 0) {
                return Identifier(dataOffs, index.fileId);
            }
        }

        return std::nullopt;
    }

    void ForEach(Engine::Session& session, std::function<bool(Data&)> action) const
    {
        static_assert(std::is_same_v<Data, FieldDefinition> || std::is_same_v<Data, MethodDefinition>);

        auto [_, raf] = session.File(index.fileId);

        for (uint32_t i = 0; i < index.bucketsSize; i++) {
            auto offset   = Offset<Data>(ReadAt(raf, index.bucketsStart + i * sizeof(uint32_t)));
            auto fieldDef = Reader::Read(session, index.fileId, offset);

            if (action(fieldDef)) {
                break;
            }
        }
    }

    std::vector<Identifier> FindOffsets(Engine::Session& session, String name) const
    {
        if (IsEmpty()) {
            return {};
        }

        auto [_, raf] = session.File(index.fileId);

        uint32_t startIdx = Hash(name) % BucketCount();
        uint32_t step     = sizeof(uint32_t);

        auto bucketStartOffs = index.bucketTableStart + startIdx * step;
        auto bucketEndOffs   = index.bucketTableStart + (startIdx + 1) * step;

        auto dataStartIdx = ReadAt(raf, bucketStartOffs);
        auto dataEndIdx   = ReadAt(raf, bucketEndOffs);

        ASSERT(dataStartIdx <= dataEndIdx);
        std::vector<Identifier> offsets;
        for (auto i = dataStartIdx; i < dataEndIdx; i++) {
            auto dataOffs   = Offset<Data>(ReadAt(raf, index.bucketsStart + i * step));
            auto entityName = Reader::ReadName(session, index.fileId, dataOffs);

            if (entityName.compare(name) == 0) {
                offsets.push_back(Identifier(dataOffs, index.fileId));
            }
        }

        return offsets;
    }

    uint32_t ReadAt(IO::RandomAccessFile& raf, uint32_t offs) const
    {
        return IO::StreamFileReader(raf, offs).ReadU32();
    }
};

std::optional<Engine::Identifier<TypeDefinition>> TypeIndex::FindType(
    Engine::Session& session, std::string_view typeName
) const
{
    MemberIndexWrapper<TypeDefinition> index { this->index };
    return index.FindOffset(session, typeName);
}

std::optional<Engine::Identifier<FieldDefinition>> FieldIndex::FindField(
    Engine::Session& session, std::string_view typeName
) const
{
    MemberIndexWrapper<FieldDefinition> index { this->index };
    return index.FindOffset(session, typeName);
}

void FieldIndex::ForEach(Engine::Session& session, std::function<bool(FieldDefinition&)> action) const
{
    MemberIndexWrapper<FieldDefinition> index { this->index };
    index.ForEach(session, action);
}

std::vector<Engine::Identifier<MethodDefinition>> MethodIndex::FindMethods(
    Engine::Session& session, std::string_view methodName
) const
{
    MemberIndexWrapper<MethodDefinition> index { this->index };
    return index.FindOffsets(session, methodName);
}

} // namespace Symlevel
