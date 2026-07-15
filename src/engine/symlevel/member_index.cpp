#include "member_index.h"
#include "engine/engine.h"
#include "engine/symlevel/io/random_access_file.h"
#include "io/stream_file_reader.h"
#include <cstdint>
#include <optional>
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

static uint32_t ReadAt(IO::RandomAccessFile* raf, uint32_t offs) { return IO::StreamFileReader(raf, offs).ReadU32(); }

std::optional<uint32_t> MemberIndex::Generator::operator()()
{
    if (cursor < endIdx) {
        uint32_t offs = index->bucketTableStart + cursor * sizeof(uint32_t);
        cursor++;
        return ReadAt(raf, offs);
    }
    return std::nullopt;
}

static uint32_t Hash(std::string_view name)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
    uint32_t hash       = 0;
    for (size_t i = 0; i < name.size(); i++) {
        hash = (hash << 5) - hash + (data[i] & 0xFF);
    }
    return hash;
}

MemberIndex::Generator MemberIndex::AllEntries(Engine::Session& session) const
{
    auto [_, raf] = session.File(fileId);
    return { this, &raf, 0, bucketsSize };
}

MemberIndex::Generator MemberIndex::FindBucket(Engine::Session& session, std::string_view name) const
{
    if (this->bucketsSize == 0) {
        return { nullptr, nullptr, 0, 0 };
    }

    auto [_, raf] = session.File(this->fileId);

    auto bucketCount = this->bucketTableSize - 1;

    uint32_t startIdx = Hash(name) % bucketCount;
    uint32_t step     = sizeof(uint32_t);

    auto bucketStartOffs = this->bucketTableStart + startIdx * step;
    auto bucketEndOffs   = this->bucketTableStart + (startIdx + 1) * step;

    auto dataStartIdx = ReadAt(&raf, bucketStartOffs);
    auto dataEndIdx   = ReadAt(&raf, bucketEndOffs);

    ASSERT(dataStartIdx <= dataEndIdx);

    return { this, &raf, dataStartIdx, dataEndIdx };
}

} // namespace Symlevel
