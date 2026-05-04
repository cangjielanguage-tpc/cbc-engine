#include "definitions.h"
#include "engine/identifiers.h"
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
MemberIndex MemberIndex::Read(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto bucketTableSize = reader.ReadU32();
    auto bucketsSize     = reader.ReadU32();

    auto bucketTableOffs = reader.Position();
    uint32_t bucketsOffs = bucketTableOffs + bucketTableSize * sizeof(uint32_t);

    reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));
    return { fileId, bucketTableOffs, bucketTableSize, bucketsOffs, bucketsSize };
}

template <typename Data>
struct MemberIndexWrapper {
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
        uint32_t step = sizeof(uint32_t);

        auto bucketStartIdx = index.bucketTableStart + startIdx * step;
        auto bucketEndIdx = index.bucketTableStart + (startIdx + 1) * step;

        auto dataStartIdx = IO::StreamFileReader(raf, bucketStartIdx).ReadU32();
        auto dataEndIdx = IO::StreamFileReader(raf, bucketEndIdx).ReadU32();

        ASSERT(dataStartIdx <= dataEndIdx);

        for (auto i = dataStartIdx; i < dataEndIdx; i++) {
            auto dataOffs = Offset<Data>(index.bucketsStart + i * step);
            auto dataIdx = IO::StreamFileReader(raf, dataOffs).ReadU32();
            auto entityName = Reader::ReadName(session, index.fileId, dataOffs);

            if (entityName.compare(name) == 0) {
                return Identifier(dataOffs, index.fileId);
            }
        }

        return std::nullopt;
    }

    std::vector<Identifier> FindOffsets(Engine::Session& session, String name) const
    {
        if (IsEmpty()) {
            return {};
        }

        auto [_, raf] = session.File(index.fileId);

        uint32_t startIdx = Hash(name) % BucketCount();
        uint32_t step = sizeof(uint32_t);

        auto bucketStartIdx = index.bucketTableStart + startIdx * step;
        auto bucketEndIdx = index.bucketTableStart + (startIdx + 1) * step;

        auto dataStartIdx = IO::StreamFileReader(raf, bucketStartIdx).ReadU32();
        auto dataEndIdx = IO::StreamFileReader(raf, bucketEndIdx).ReadU32();

        ASSERT(dataStartIdx <= dataEndIdx);
        std::vector<Identifier> offsets;
        for (auto i = dataStartIdx; i < dataEndIdx; i++) {
            auto dataOffs = Offset<Data>(index.bucketsStart + i * step);
            auto dataIdx = IO::StreamFileReader(raf, dataOffs).ReadU32();
            auto entityName = Reader::ReadName(session, index.fileId, dataOffs);

            if (entityName.compare(name) == 0) {
                offsets.push_back(Identifier(dataOffs, index.fileId));
            }
        }

        return offsets;
    }
};

std::optional<Engine::Identifier<TypeDefinition>> TypeIndex::FindType(Engine::Session& session, String typeName) const
{
    MemberIndexWrapper<TypeDefinition> index {this->index};
    return index.FindOffset(session, typeName);
}

std::optional<Engine::Identifier<FieldDefinition>> FieldIndex::FindField(Engine::Session& session, String typeName) const
{
    MemberIndexWrapper<FieldDefinition> index {this->index};
    return index.FindOffset(session, typeName);
}

std::vector<Engine::Identifier<MethodDefinition>> MethodIndex::FindMethods(Engine::Session& session, String methodName) const
{
    MemberIndexWrapper<MethodDefinition> index {this->index};
    return index.FindOffsets(session, methodName);
}

} // namespace Symlevel
