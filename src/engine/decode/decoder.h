#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/cbc_file.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/offset_pool.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/sequence.h"
#include <cassert>
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

    HashTableRange(IO::RandomAccessFile* raf, IO::FileId file, uint32_t startOffs, uint32_t endOffs);

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

template <typename T> struct RefSequence {
    Symlevel::RefSequence<T> seq;
    IO::RandomAccessFile* file;

    RefSequence(Symlevel::RefSequence<T> seq, IO::RandomAccessFile* file) : seq(seq), file(file) {}

    RefSequence() : seq() {}

    struct Sentinel {};

    struct Iterator {
        IO::StreamFileReader reader;
        IO::FileId fileId;
        uint32_t endPos;
        long long value = -1;

        RefIdentifier<T> operator*() const { return RefIdentifier<T>(Symlevel::RefId<T>(value), fileId); }

        Iterator& operator++()
        {
            value = reader.ReadULEB();
            return *this;
        }

        bool operator!=(Sentinel) const { return reader.Position() != endPos; }
    };

    Iterator begin() const
    {
        IO::StreamFileReader reader(file, seq.startPos);
        long long value = -1;
        if (seq.startPos != seq.endPos) {
            value = reader.ReadULEB();
        }
        return Iterator { reader, seq.file, seq.endPos, value };
    }

    Sentinel end() const { return {}; }
};

template <typename T> struct OffsetSequence {
    Symlevel::OffsetSequence<T> seq;
    IO::RandomAccessFile* file;

    OffsetSequence(Symlevel::OffsetSequence<T> seq, IO::RandomAccessFile* file) : seq(seq), file(file) {}

    OffsetSequence() : seq() {}

    struct Sentinel {};

    struct Iterator {
        IO::StreamFileReader reader;
        IO::FileId fileId;
        uint32_t endPos;
        long long value = -1;

        Identifier<T> operator*() const { return Identifier<T>(Symlevel::Offset<T>(value), fileId); }

        Iterator& operator++()
        {
            value = reader.ReadULEB();
            return *this;
        }

        bool operator!=(Sentinel) const { return reader.Position() != endPos; }
    };

    Iterator begin() const
    {
        IO::StreamFileReader reader(file, seq.startPos);
        long long value = -1;
        if (seq.startPos != seq.endPos) {
            value = reader.ReadULEB();
        }
        return Iterator { reader, seq.file, seq.endPos, value };
    }

    Sentinel end() const { return {}; }
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

    template <typename T> RefSequence<T> Resolve(Symlevel::RefSequence<T> seq)
    {
        return RefSequence<T>(seq, session.FileOf(seq.file).get());
    }

    template <typename T> OffsetSequence<T> Resolve(Symlevel::OffsetSequence<T> seq)
    {
        return OffsetSequence<T>(seq, session.FileOf(seq.file).get());
    }

    template <typename T> Identifier<T> Resolve(RefIdentifier<T> id)
    {
        auto [file, raf] = session.File(id.GetFileId());
        auto pool        = file.GetRegionData().template ErasedPool<T>();
        auto index       = id.GetIndex() - pool.adjustment;
        assert(index < pool.size);
        uint32_t offset = raf.ReadU32(pool.offset + index * sizeof(uint32_t));
        return Identifier<T>(Symlevel::Offset<T>(offset), id.GetFileId());
    }
};

Symlevel::MemberIndex<void> ReadIndex(IO::StreamFileReader& reader, IO::FileId file);

} // namespace Decode
