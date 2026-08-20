#pragma once

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/image/cbc_file.h"

namespace Decode {

inline namespace Reader {
template <typename T> T Read(Engine::Session& session, Image::FileId fileId, Image::Offset<T> offset);
template <typename T> T Read(Engine::Session& session, Image::Identifier<T> id);

std::vector<Image::ExceptionRegion> GetExceptionRegions(Engine::Session& session, Image::Code const& code);
std::vector<Image::LivenessInfo> GetLivenessInfo(Engine::Session& session, Image::Code const& code);
std::vector<Image::StackPtrsInfo> GetStackPtrsInfo(Engine::Session& session, Image::Code const& code);

template <typename T> Image::RefSequence<T> ReadRefSeq(IO::StreamFileReader& reader, Image::FileId file)
{
    auto size     = reader.ReadULEB();
    auto startPos = reader.Position();
    auto endPos   = startPos + size;
    reader.Advance(size);
    return Image::RefSequence<T>(file, startPos, endPos);
}

template <typename T> Image::OffsetSequence<T> ReadOffsSeq(IO::StreamFileReader& reader, Image::FileId file)
{
    auto size     = reader.ReadULEB();
    auto startPos = reader.Position();
    auto endPos   = startPos + size;
    reader.Advance(size);
    return Image::OffsetSequence<T>(file, startPos, endPos);
}

Image::MethodReference Read(Engine::Session& session, Image::RefIdentifier<Image::MethodReference> id);
Image::FieldReference Read(Engine::Session& session, Image::RefIdentifier<Image::FieldReference> id);

template <typename T> Decode::HashTableRange<T> AllEntries(Engine::Session& s, Image::MemberIndex<T> const& index)
{
    return s.Decoder().AllEntries(index);
}

template <typename T>
Decode::Bucket<T> FindBucket(Engine::Session& s, Image::MemberIndex<T> const& index, std::string_view name)
{
    return s.Decoder().FindBucket(index, name);
}

template <typename T>
std::optional<Identifier<T>> Find(Engine::Session& s, Image::MemberIndex<T> const& index, std::string_view name)
{
    return s.Decoder().Find(index, name);
}

template <typename T> T GetAotData(Engine::Session& s, RefIdentifier<Image::MethodReference> index)
{
    return s.Decoder().GetAotData<T>(index);
}

template <typename T> T GetAotData(Engine::Session& s, RefIdentifier<Image::FieldReference> index)
{
    return s.Decoder().GetAotData<T>(index);
}

template <typename T> Decode::RefSequence<T> Resolve(Engine::Session& s, Image::RefSequence<T> seq)
{
    return s.Decoder().Resolve(seq);
}

template <typename T> Decode::OffsetSequence<T> Resolve(Engine::Session& s, Image::OffsetSequence<T> seq)
{
    return s.Decoder().Resolve(seq);
}

Image::RegionData ReadRegion(Image::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

Image::MemberIndex<void> ReadIndex(IO::StreamFileReader& reader, FileId file);
}; // namespace Reader

} // namespace Decode
