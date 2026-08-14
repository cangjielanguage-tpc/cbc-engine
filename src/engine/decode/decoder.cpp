

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/random_access_file.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "engine/symlevel/member_index.h"
#include "engine/symlevel/offset.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/references.h"
#include "engine/symlevel/string.h"
#include "utils/assertion.h"
#include <cstdint>
#include <optional>
#include <string_view>

namespace Decode {

static constexpr size_t OFFSET_ADJUSTMENT = 57;

// ------------------ Utilities ------------------

template <typename K> struct Hash;

template <> struct Hash<std::string_view> {
    uint64_t operator()(std::string_view name)
    {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(name.data());
        uint32_t hash       = 0;
        for (size_t i = 0; i < name.size(); i++) {
            hash = (hash << 5) - hash + (data[i] & 0xFF);
        }
        return hash;
    }
};

template <> struct Hash<uint32_t> {
    uint64_t operator()(uint32_t name) { return name; }
};

static uint32_t ReadAt(IO::RandomAccessFile* raf, uint32_t offs) { return IO::StreamFileReader(raf, offs).ReadU32(); }

// ------------------ Decoder ------------------

Decoder::Decoder(Engine::Session& session) : session(session) {}

template <typename T> HashTableRange<T> Decoder::AllEntries(Symlevel::MemberIndex<T> const& index)
{
    return HashTableRange<T>(
        session.FileOf(index.fileId).get(),
        index.fileId,
        index.bucketsStart,
        index.bucketsStart + sizeof(uint32_t) * index.bucketsSize
    );
}

template <typename T, typename Key>
static HashTableRange<T> FindBucketRange(IO::RandomAccessFile* file, Symlevel::MemberIndex<T> const& index, Key key)
{
    if (index.bucketsSize == 0) {
        return HashTableRange<T>(file, index.fileId, 0, 0);
    }

    auto bucketCount = index.bucketTableSize - 1;

    Hash<Key> h;
    uint32_t startIdx = h(key) % bucketCount;
    uint32_t step     = sizeof(uint32_t);

    auto bucketStartOffs = index.bucketTableStart + startIdx * step;
    auto bucketEndOffs   = index.bucketTableStart + (startIdx + 1) * step;

    auto dataStartOffs = index.bucketsStart + step * ReadAt(file, bucketStartOffs);
    auto dataEndOffs   = index.bucketsStart + step * ReadAt(file, bucketEndOffs);

    ASSERT(dataStartOffs <= dataEndOffs);
    return HashTableRange<T>(file, index.fileId, dataStartOffs, dataEndOffs);
}

template <typename T> Bucket<T> Decoder::FindBucket(Symlevel::MemberIndex<T> const& index, std::string_view name)
{
    auto file = this->session.FileOf(index.fileId).get();
    return Bucket(FindBucketRange(file, index, name), name);
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

// ------------------ AOT data decoding ------------------

template <typename T>
static Identifier<T> FindAotData(IO::RandomAccessFile* file, uint32_t id, Symlevel::MemberIndex<T> const* index)
{
    for (auto v : FindBucketRange<T>(file, *index, id)) {
        auto offset  = v.GetOffset();
        auto entryId = ReadAt(file, offset + OFFSET_ADJUSTMENT);
        if (id == entryId) {
            return v;
        }
    }
    FATAL("Incorrect encoding of aot data");
}

template <>
Symlevel::DirectCallAotData Decoder::GetAotData<Symlevel::DirectCallAotData>(
    RefIdentifier<Symlevel::MethodReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetDirectCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    auto name = Symlevel::Offset<Symlevel::String>(reader.ReadU32());
    return { Engine::Identifier(name, index.GetFileId()) };
}

template <>
Symlevel::VirtualCallAotData Decoder::GetAotData<Symlevel::VirtualCallAotData>(
    RefIdentifier<Symlevel::MethodReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetVirtualCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU16(), reader.ReadU16() };
}

template <>
Symlevel::InterfaceCallAotData Decoder::GetAotData<Symlevel::InterfaceCallAotData>(
    RefIdentifier<Symlevel::MethodReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetInterfaceCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU16() };
}

template <>
Symlevel::StaticFieldAotData Decoder::GetAotData<Symlevel::StaticFieldAotData>(
    RefIdentifier<Symlevel::FieldReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetStaticFieldAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    auto name = Symlevel::Offset<Symlevel::String>(reader.ReadU32());
    return { Engine::Identifier(name, index.GetFileId()) };
}

template <>
Symlevel::InstanceFieldAotData Decoder::GetAotData<Symlevel::InstanceFieldAotData>(
    RefIdentifier<Symlevel::FieldReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetStaticFieldAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU32() };
}

// ------------------ Hash table range ------------------

template <typename T>
HashTableRange<T>::HashTableRange(IO::RandomAccessFile* raf, IO::FileId file, uint32_t startOffs, uint32_t endOffs)
    : raf(raf),
      file(file),
      startOffs(startOffs),
      endOffs(endOffs)
{}

template <typename T> typename HashTableRange<T>::Iterator HashTableRange<T>::begin() const
{
    return { raf, startOffs, file };
}

template <typename T> typename HashTableRange<T>::Iterator HashTableRange<T>::end() const
{
    return { nullptr, endOffs, file };
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

template <typename T>
static bool CompareName(IO::RandomAccessFile* file, Symlevel::Offset<T> offs, std::string_view str)
{
    IO::StreamFileReader reader(file, OFFSET_ADJUSTMENT + offs);
    auto strOffs = Symlevel::Offset<Symlevel::String>(reader.ReadU32());
    IO::StreamFileReader stringReader(file, OFFSET_ADJUSTMENT + strOffs);
    uint32_t size = stringReader.ReadULEB();
    if (str.size() != size) {
        return false;
    }

    // Compare without allocating new memory.
    char buffer[256];
    while (size > 0) {
        uint32_t chunkSize = size < sizeof(buffer) ? size : sizeof(buffer);
        stringReader.Read(buffer, chunkSize);
        auto chunk = str.substr(0, chunkSize);
        if (chunk.compare(std::string_view(buffer, chunkSize)) != 0) {
            return false;
        }
        size -= chunkSize;
        str   = str.substr(chunkSize);
    }
    return true;
}

template <typename T> static void skipUntilEqualKey(typename Bucket<T>::Iterator& it)
{
    auto file = it.bucket->range.raf;
    while (it.cursor < it.bucket->range.endOffs) {
        auto offs = ReadAt(it.file, it.cursor);
        if (CompareName<T>(file, Symlevel::Offset<T>(offs), it.bucket->key)) {
            return;
        }
        it.cursor += sizeof(uint32_t);
    }
}

template <typename T> Bucket<T>::Bucket(HashTableRange<T> range, std::string_view key) : range(range), key(key) {}

template <typename T> typename Bucket<T>::Iterator Bucket<T>::begin() const
{
    Bucket<T>::Iterator iterator { this, range.raf, range.startOffs };
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
