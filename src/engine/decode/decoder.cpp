

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/reader.h"
#include <cstdint>
#include <optional>

namespace Decode {

// ------------------ Utilities ------------------

static uint32_t HashString(std::string_view name)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
    uint32_t hash       = 0;
    for (size_t i = 0; i < name.size(); i++) {
        hash = (hash << 5) - hash + (data[i] & 0xFF);
    }
    return hash;
}

static uint32_t ReadAt(IO::RandomAccessFile* raf, uint32_t offs) { return IO::StreamFileReader(raf, offs).ReadU32(); }

// ------------------ Decoder ------------------

Decoder::Decoder(Engine::Session& session) : session(session) {}

template <typename T> HashTableRange<T> Decoder::AllEntries(Symlevel::MemberIndex<T> const& index)
{
    return HashTableRange<T>(this, index.fileId, 0, index.bucketsSize);
}

template <typename T> Bucket<T> Decoder::FindBucket(Symlevel::MemberIndex<T> const& index, std::string_view name)
{
    if (index.bucketsSize == 0) {
        return Bucket(HashTableRange<T>(this, index.fileId, 0, index.bucketsSize), name);
    }

    auto [_, raf] = session.File(index.fileId);

    auto bucketCount = index.bucketTableSize - 1;

    uint32_t startIdx = HashString(name) % bucketCount;
    uint32_t step     = sizeof(uint32_t);

    auto bucketStartOffs = index.bucketTableStart + startIdx * step;
    auto bucketEndOffs   = index.bucketTableStart + (startIdx + 1) * step;

    auto dataStartOffs = step * ReadAt(&raf, bucketStartOffs);
    auto dataEndOffs   = step * ReadAt(&raf, bucketEndOffs);

    ASSERT(dataStartOffs <= dataEndOffs);
    return Bucket(HashTableRange<T>(this, index.fileId, dataStartOffs, dataEndOffs), name);
}

template <typename T>
std::optional<Engine::Identifier<T>> Decoder::Find(Symlevel::MemberIndex<T> const& index, std::string_view name)
{
    for (auto value : FindBucket(index, name)) {
        return value;
    }
    return std::nullopt;
}

Symlevel::MemberIndex<void> ReadIndex(IO::StreamFileReader& reader, IO::FileId file)
{
    auto bucketTableSize = reader.ReadU32();
    auto bucketsSize     = reader.ReadU32();
    auto bucketTableOffs = reader.Position();
    uint32_t bucketsOffs = bucketTableOffs + bucketTableSize * sizeof(uint32_t);
    reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));
    return Symlevel::MemberIndex<void>(file, bucketTableOffs, bucketTableSize, bucketsOffs, bucketsSize);
}

// ------------------ Hash table range ------------------

template <typename T>
HashTableRange<T>::HashTableRange(struct Decoder* decoder, IO::FileId file, uint32_t startOffs, uint32_t endOffs)
    : decoder(decoder),
      file(file),
      startOffs(startOffs),
      endOffs(endOffs)
{}

template <typename T> typename HashTableRange<T>::Iterator HashTableRange<T>::begin() const
{
    auto& raf = decoder->session.FileOf(file);
    return { decoder, raf.get(), startOffs, file };
}

template <typename T> typename HashTableRange<T>::Iterator HashTableRange<T>::end() const
{
    return { decoder, nullptr, endOffs, file };
}

template <typename T> Engine::Identifier<T> HashTableRange<T>::Iterator::operator*() const
{
    auto offs = ReadAt(file, cursor);
    return Engine::Identifier<T>(Symlevel::Offset<T>(offs), fileId);
}

template <typename T> typename HashTableRange<T>::Iterator& HashTableRange<T>::Iterator::operator++()
{
    cursor += sizeof(uint32_t);
    return *this;
}

template <typename T> typename HashTableRange<T>::Iterator HashTableRange<T>::Iterator::operator++(int)
{
    auto res  = *this;
    cursor   += sizeof(uint32_t);
    return res;
}

template <typename T> bool HashTableRange<T>::Iterator::operator==(HashTableRange<T>::Iterator const& another) const
{
    return cursor == another.cursor;
}

// ------------------ Bucket ------------------

template <typename T> static void skipUntilEqualKey(typename Bucket<T>::Iterator& it)
{
    auto& session = it.bucket->range.decoder->session;
    auto file     = it.bucket->range.file;
    while (it.cursor < it.bucket->range.endOffs) {
        auto offs = ReadAt(it.file, it.cursor);
        auto name = Symlevel::Reader::ReadName(session, file, Symlevel::Offset<T>(offs));
        if (it.bucket->key.compare(name) == 0) {
            return;
        }
        it.cursor += sizeof(uint32_t);
    }
}

template <typename T> Bucket<T>::Bucket(HashTableRange<T> range, std::string_view key) : range(range), key(key) {}

template <typename T> typename Bucket<T>::Iterator Bucket<T>::begin() const
{
    auto& raf = range.decoder->session.FileOf(range.file);
    Bucket<T>::Iterator iterator { this, raf.get(), range.startOffs };
    skipUntilEqualKey<T>(iterator);
    return iterator;
}

template <typename T> typename Bucket<T>::Iterator Bucket<T>::end() const
{
    return { nullptr, nullptr, range.endOffs };
}

template <typename T> Engine::Identifier<T> Bucket<T>::Iterator::operator*() const
{
    auto offs = ReadAt(file, cursor);
    return Engine::Identifier<T>(Symlevel::Offset<T>(offs), bucket->range.file);
}

template <typename T> typename Bucket<T>::Iterator& Bucket<T>::Iterator::operator++()
{
    cursor += sizeof(uint32_t);
    return *this;
}

template <typename T> typename Bucket<T>::Iterator Bucket<T>::Iterator::operator++(int)
{
    auto res  = *this;
    cursor   += sizeof(uint32_t);
    return res;
}

template <typename T> bool Bucket<T>::Iterator::operator==(Bucket<T>::Iterator const& another) const
{
    return cursor == another.cursor;
}

// ------------------ Specializations ------------------

using TD = Symlevel::TypeDefinition;
using MD = Symlevel::MethodDefinition;
using FD = Symlevel::FieldDefinition;

template HashTableRange<TD> Decoder::AllEntries(Symlevel::MemberIndex<TD> const& index);
template HashTableRange<MD> Decoder::AllEntries(Symlevel::MemberIndex<MD> const& index);
template HashTableRange<FD> Decoder::AllEntries(Symlevel::MemberIndex<FD> const& index);
template Bucket<TD> Decoder::FindBucket(Symlevel::MemberIndex<TD> const& index, std::string_view name);
template Bucket<MD> Decoder::FindBucket(Symlevel::MemberIndex<MD> const& index, std::string_view name);
template Bucket<FD> Decoder::FindBucket(Symlevel::MemberIndex<FD> const& index, std::string_view name);

template std::optional<Engine::Identifier<TD>> Decoder::Find(
    Symlevel::MemberIndex<TD> const& index, std::string_view name
);
template std::optional<Engine::Identifier<MD>> Decoder::Find(
    Symlevel::MemberIndex<MD> const& index, std::string_view name
);
template std::optional<Engine::Identifier<FD>> Decoder::Find(
    Symlevel::MemberIndex<FD> const& index, std::string_view name
);

template struct HashTableRange<TD>;
template struct HashTableRange<MD>;
template struct HashTableRange<FD>;
template struct Bucket<TD>;
template struct Bucket<MD>;
template struct Bucket<FD>;

} // namespace Decode
