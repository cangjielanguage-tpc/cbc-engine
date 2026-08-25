

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/image/cbc_file.h"
#include "engine/image/io/random_access_file.h"
#include "engine/image/io/stream_file_reader.h"
#include "reader.h"
#include "utils/assertion.h"
#include <cstdint>
#include <optional>
#include <string_view>

namespace Decode {

static constexpr size_t OFFSET_ADJUSTMENT = Image::POOL_OFFSET_ADJUSTMENT;

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

static uint32_t ReadAt(IO::RandomAccessFile* raf, uint32_t offs) { return raf->ReadU32(offs); }

// ------------------ Decoder ------------------

Decoder::Decoder(Engine::Session& session) : session(session) {}

template <typename T> HashTableRange<T> Decoder::AllEntries(Image::MemberIndex<T> const& index)
{
    return HashTableRange<T>(
        session.FileOf(index.fileId).get(),
        index.fileId,
        index.bucketsStart,
        index.bucketsStart + sizeof(uint32_t) * index.bucketsSize
    );
}

template <typename T, typename Key>
static HashTableRange<T> FindBucketRange(IO::RandomAccessFile* file, Image::MemberIndex<T> const& index, Key key)
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

template <typename T> Bucket<T> Decoder::FindBucket(Image::MemberIndex<T> const& index, std::string_view name)
{
    auto file = this->session.FileOf(index.fileId).get();
    return Bucket(FindBucketRange(file, index, name), name);
}

template <typename T>
std::optional<Image::Identifier<T>> Decoder::Find(Image::MemberIndex<T> const& index, std::string_view name)
{
    auto bucket = FindBucket(index, name);
    for (auto value : bucket) {
        return value;
    }
    return std::nullopt;
}

Image::MemberIndex<void> Reader::ReadIndex(IO::StreamFileReader& reader, FileId file)
{
    auto bucketTableSize = reader.ReadU32();
    auto bucketsSize     = reader.ReadU32();
    auto bucketTableOffs = reader.Position();
    uint32_t bucketsOffs = bucketTableOffs + bucketTableSize * sizeof(uint32_t);
    reader.Advance(bucketTableSize * sizeof(uint32_t) + bucketsSize * sizeof(uint32_t));
    return Image::MemberIndex<void>(file, bucketTableOffs, bucketTableSize, bucketsOffs, bucketsSize);
}

// ------------------ AOT data decoding ------------------

template <typename T>
static Identifier<T> FindAotData(IO::RandomAccessFile* file, uint32_t id, Image::MemberIndex<T> const* index)
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
Image::DirectCallAotData Decoder::GetAotData<Image::DirectCallAotData>(RefIdentifier<Image::MethodReference> index)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetDirectCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    auto name = Image::Offset<Image::String>(reader.ReadU32());
    return { Image::Identifier(name, index.GetFileId()) };
}

template <>
Image::VirtualCallAotData Decoder::GetAotData<Image::VirtualCallAotData>(RefIdentifier<Image::MethodReference> index)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetVirtualCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU16(), reader.ReadU16() };
}

template <>
Image::InterfaceCallAotData Decoder::GetAotData<Image::InterfaceCallAotData>(RefIdentifier<Image::MethodReference> index
)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetInterfaceCallAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU16() };
}

template <>
Image::StaticFieldAotData Decoder::GetAotData<Image::StaticFieldAotData>(RefIdentifier<Image::FieldReference> index)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetStaticFieldAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    auto name = Image::Offset<Image::String>(reader.ReadU32());
    return { Image::Identifier(name, index.GetFileId()) };
}

template <>
Image::InstanceFieldAotData Decoder::GetAotData<Image::InstanceFieldAotData>(RefIdentifier<Image::FieldReference> index)
{
    auto [cbc, raf] = session.File(index.GetFileId());
    auto id         = FindAotData(&raf, index.GetIndex(), &cbc.GetInstanceFieldAotTable());

    // skip index
    IO::StreamFileReader reader(raf, OFFSET_ADJUSTMENT + id.GetOffset() + 4);
    return { reader.ReadU32() };
}

// ------------------ Hash table range ------------------

template <typename T>
HashTableRange<T>::HashTableRange(IO::RandomAccessFile* raf, FileId file, uint32_t startOffs, uint32_t endOffs)
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

template <typename T> Image::Identifier<T> HashTableRange<T>::Iterator::operator*() const
{
    auto offs = ReadAt(file, cursor);
    return Image::Identifier<T>(Image::Offset<T>(offs), fileId);
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

template <typename T> static bool CompareName(IO::RandomAccessFile* file, Image::Offset<T> offs, std::string_view str)
{
    IO::StreamFileReader reader(file, OFFSET_ADJUSTMENT + offs);
    auto strOffs = Image::Offset<Image::String>(reader.ReadU32());
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
        if (CompareName<T>(file, Image::Offset<T>(offs), it.bucket->key)) {
            it.value = offs;
            return;
        }
        it.cursor += sizeof(uint32_t);
    }
    it.value = -1;
}

template <typename T> Bucket<T>::Bucket(HashTableRange<T> range, std::string_view key) : range(range), key(key) {}

template <typename T> typename Bucket<T>::Iterator Bucket<T>::begin() const
{
    Bucket<T>::Iterator iterator { this, range.raf, range.startOffs, -1 };
    skipUntilEqualKey<T>(iterator);
    return iterator;
}

template <typename T> Image::Identifier<T> Bucket<T>::Iterator::operator*() const
{
    return Image::Identifier<T>(Image::Offset<T>(value), bucket->range.file);
}

template <typename T> typename Bucket<T>::Iterator& Bucket<T>::Iterator::operator++()
{
    cursor += sizeof(uint32_t);
    skipUntilEqualKey<T>(*this);
    return *this;
}

// ------------------ Specializations ------------------

using TD = Image::TypeDefinition;
using MD = Image::MethodDefinition;
using FD = Image::FieldDefinition;

template HashTableRange<TD> Decoder::AllEntries(Image::MemberIndex<TD> const& index);
template HashTableRange<MD> Decoder::AllEntries(Image::MemberIndex<MD> const& index);
template HashTableRange<FD> Decoder::AllEntries(Image::MemberIndex<FD> const& index);
template Bucket<TD> Decoder::FindBucket(Image::MemberIndex<TD> const& index, std::string_view name);
template Bucket<MD> Decoder::FindBucket(Image::MemberIndex<MD> const& index, std::string_view name);
template Bucket<FD> Decoder::FindBucket(Image::MemberIndex<FD> const& index, std::string_view name);

template std::optional<Image::Identifier<TD>> Decoder::Find(Image::MemberIndex<TD> const& index, std::string_view name);
template std::optional<Image::Identifier<MD>> Decoder::Find(Image::MemberIndex<MD> const& index, std::string_view name);
template std::optional<Image::Identifier<FD>> Decoder::Find(Image::MemberIndex<FD> const& index, std::string_view name);

template struct HashTableRange<TD>;
template struct HashTableRange<MD>;
template struct HashTableRange<FD>;
template struct Bucket<TD>;
template struct Bucket<MD>;
template struct Bucket<FD>;

} // namespace Decode
