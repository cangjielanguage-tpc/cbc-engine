#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/references.h"
#include <cstdint>
#include <string_view>

namespace Decode {

template <typename T> using Identifier = Engine::Identifier<T>;
template <typename T> using RefIdentifier = Engine::RefIdentifier<T>;

// MemberIndex and AotData table are encoded using same format.
// This table consists of buckets, where each entry with the same hash
// are stored in the same bucket.
// So, any query must find a range, where it can traverse linearly further.
template <typename T> struct HashTableRange {
    IO::RandomAccessFile* raf;
    IO::FileId file;
    uint32_t startOffs;
    uint32_t endOffs;

    HashTableRange(IO::RandomAccessFile* decoder, IO::FileId file, uint32_t startOffs, uint32_t endOffs);

    struct Iterator {
        IO::RandomAccessFile* file;
        uint32_t cursor;
        IO::FileId fileId;

        using iterator_category = std::input_iterator_tag;
        using value_type        = Identifier<T>;
        using difference_type   = std::ptrdiff_t;
        using reference         = value_type;
        using pointer           = void;

        value_type operator*() const;
        Iterator& operator++();
        Iterator operator++(int);
        bool operator==(Iterator const&) const;

        bool operator!=(Iterator const& another) const { return !(*this == another); }
    };

    Iterator begin() const;
    Iterator end() const;
};

// This wrapper is provided for convinient iteration through elements in bucket.
template <typename T> struct Bucket {
    HashTableRange<T> range;
    std::string_view key;

    Bucket(HashTableRange<T> range, std::string_view key);

    struct Iterator {
        Bucket const* bucket;
        IO::RandomAccessFile* file;
        uint32_t cursor;

        using iterator_category = std::input_iterator_tag;
        using value_type        = Identifier<T>;
        using difference_type   = std::ptrdiff_t;
        using reference         = value_type;
        using pointer           = void;

        value_type operator*() const;
        Iterator& operator++();
        Iterator operator++(int);
        bool operator==(Iterator const&) const;

        bool operator!=(Iterator const& another) const { return !(*this == another); }
    };

    Iterator begin() const;
    Iterator end() const;
};

struct Decoder {
    Engine::Session& session;

    Decoder(Engine::Session& session);
    Decoder(Decoder const& another) = delete;

    template <typename T> HashTableRange<T> AllEntries(Symlevel::MemberIndex<T> const& index);

    template <typename T> Bucket<T> FindBucket(Symlevel::MemberIndex<T> const& index, std::string_view name);

    template <typename T>
    std::optional<Identifier<T>> Find(Symlevel::MemberIndex<T> const& index, std::string_view name);

    template <typename T> T GetAotData(RefIdentifier<Symlevel::MethodReference> index);
    template <typename T> T GetAotData(RefIdentifier<Symlevel::FieldReference> index);
};

Symlevel::MemberIndex<void> ReadIndex(IO::StreamFileReader& reader, IO::FileId file);

} // namespace Decode
