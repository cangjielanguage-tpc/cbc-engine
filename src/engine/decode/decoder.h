#pragma once

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/image/cbc_file.h"
#include "engine/image/io/random_access_file.h"
#include "engine/image/io/stream_file_reader.h"
#include <string_view>

namespace Decode {

template <typename T> using Identifier    = Image::Identifier<T>;
template <typename T> using RefIdentifier = Image::RefIdentifier<T>;
using FileId                              = Image::FileId;

// MemberIndex and AotData table are encoded using same format.
// This table consists of buckets, where each entry with the same hash
// are stored in the same bucket.
// So, any query must find a range, where it can traverse linearly further.
template <typename T> struct HashTableRange {
    IO::RandomAccessFile* raf;
    FileId file;
    uint32_t startOffs;
    uint32_t endOffs;

    HashTableRange(IO::RandomAccessFile* raf, FileId file, uint32_t startOffs, uint32_t endOffs);

    struct Iterator {
        IO::RandomAccessFile* file;
        uint32_t cursor;
        FileId fileId;

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

    struct Sentinel {};

    struct Iterator {
        Bucket const* bucket;
        IO::RandomAccessFile* file;
        uint32_t cursor;
        long long value;

        Identifier<T> operator*() const;
        Iterator& operator++();

        bool operator!=(Sentinel) const { return value >= 0; }
    };

    Iterator begin() const;

    Sentinel end() const { return {}; }
};

template <typename T> struct RefSequence {
    Image::RefSequence<T> seq;
    IO::RandomAccessFile* file;

    RefSequence(Image::RefSequence<T> seq, IO::RandomAccessFile* file) : seq(seq), file(file) {}

    RefSequence() : seq() {}

    struct Sentinel {};

    struct Iterator {
        IO::StreamFileReader reader;
        FileId fileId;
        uint32_t endPos;
        long long value = -1;

        RefIdentifier<T> operator*() const { return RefIdentifier<T>(Image::RefId<T>(value), fileId); }

        Iterator& operator++()
        {
            if (reader.Position() < endPos) {
                value = reader.ReadULEB();
            } else {
                value = -1;
            }
            return *this;
        }

        bool operator!=(Sentinel) const { return value >= 0; }
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
    Image::OffsetSequence<T> seq;
    IO::RandomAccessFile* file;

    OffsetSequence(Image::OffsetSequence<T> seq, IO::RandomAccessFile* file) : seq(seq), file(file) {}

    OffsetSequence() : seq() {}

    struct Sentinel {};

    struct Iterator {
        IO::StreamFileReader reader;
        FileId fileId;
        uint32_t endPos;
        long long value = -1;

        Identifier<T> operator*() const { return Identifier<T>(Image::Offset<T>(value), fileId); }

        Iterator& operator++()
        {
            if (reader.Position() < endPos) {
                value = reader.ReadULEB();
            } else {
                value = -1;
            }
            return *this;
        }

        bool operator!=(Sentinel) const { return value >= 0; }
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

    template <typename T> HashTableRange<T> AllEntries(Image::MemberIndex<T> const& index);

    template <typename T> Bucket<T> FindBucket(Image::MemberIndex<T> const& index, std::string_view name);

    template <typename T> std::optional<Identifier<T>> Find(Image::MemberIndex<T> const& index, std::string_view name);

    template <typename T> T GetAotData(RefIdentifier<Image::MethodReference> index);
    template <typename T> T GetAotData(RefIdentifier<Image::FieldReference> index);

    template <typename T> RefSequence<T> Resolve(Image::RefSequence<T> seq)
    {
        return RefSequence<T>(seq, session.FileOf(seq.file).get());
    }

    template <typename T> OffsetSequence<T> Resolve(Image::OffsetSequence<T> seq)
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
        return Identifier<T>(Image::Offset<T>(offset), id.GetFileId());
    }
};

} // namespace Decode
