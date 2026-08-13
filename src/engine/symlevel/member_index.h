#pragma once

#include "engine/symlevel/io/file_id.h"
#include <cstdint>

namespace Symlevel {

class TypeDefinition;
class MethodDefinition;
class FieldDefinition;

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
template <typename T> struct MemberIndex {
    IO::FileId fileId;

    uint32_t bucketTableStart;
    uint32_t bucketTableSize;

    uint32_t bucketsStart;
    uint32_t bucketsSize;

    MemberIndex(
        IO::FileId fileId,
        uint32_t bucketTableStart,
        uint32_t bucketTableSize,
        uint32_t bucketsStart,
        uint32_t bucketsSize
    )
        : fileId(fileId),
          bucketTableStart(bucketTableStart),
          bucketTableSize(bucketTableSize),
          bucketsStart(bucketsStart),
          bucketsSize(bucketsSize)
    {}

    MemberIndex(MemberIndex<void> const& index)
        : fileId(index.fileId),
          bucketTableStart(index.bucketTableStart),
          bucketTableSize(index.bucketTableSize),
          bucketsStart(index.bucketsStart),
          bucketsSize(index.bucketsSize)
    {}
};

using TypeIndex   = MemberIndex<TypeDefinition>;
using FieldIndex  = MemberIndex<FieldDefinition>;
using MethodIndex = MemberIndex<MethodDefinition>;

} // namespace Symlevel
