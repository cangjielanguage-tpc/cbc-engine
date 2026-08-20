#pragma once

#include "engine/decode/decoder.h"
#include "engine/engine.h"

namespace Image {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, Image::FileId fileId, Offset<T> offset);
    template <typename T> static T Read(Engine::Session& session, Image::Identifier<T> id);

    static std::vector<ExceptionRegion> GetExceptionRegions(Engine::Session& session, Code const& code);
    static std::vector<LivenessInfo> GetLivenessInfo(Engine::Session& session, Code const& code);
    static std::vector<StackPtrsInfo> GetStackPtrsInfo(Engine::Session& session, Code const& code);

    template <typename T> static RefSequence<T> ReadRefSeq(IO::StreamFileReader& reader, Image::FileId file)
    {
        auto size     = reader.ReadULEB();
        auto startPos = reader.Position();
        auto endPos   = startPos + size;
        reader.Advance(size);
        return RefSequence<T>(file, startPos, endPos);
    }

    template <typename T> static OffsetSequence<T> ReadOffsSeq(IO::StreamFileReader& reader, Image::FileId file)
    {
        auto size     = reader.ReadULEB();
        auto startPos = reader.Position();
        auto endPos   = startPos + size;
        reader.Advance(size);
        return OffsetSequence<T>(file, startPos, endPos);
    }

    static MethodReference Read(Engine::Session& session, Image::RefIdentifier<MethodReference> id);
    static FieldReference Read(Engine::Session& session, Image::RefIdentifier<FieldReference> id);

    template <typename T>
    static Decode::HashTableRange<T> AllEntries(Engine::Session& s, Image::MemberIndex<T> const& index)
    {
        return s.Decoder().AllEntries(index);
    }

    template <typename T>
    static Decode::Bucket<T> FindBucket(
        Engine::Session& s, Image::MemberIndex<T> const& index, std::string_view name
    )
    {
        return s.Decoder().FindBucket(index, name);
    }

    template <typename T>
    static std::optional<Identifier<T>> Find(
        Engine::Session& s, Image::MemberIndex<T> const& index, std::string_view name
    )
    {
        return s.Decoder().Find(index, name);
    }

    template <typename T> static T GetAotData(Engine::Session& s, RefIdentifier<Image::MethodReference> index)
    {
        return s.Decoder().GetAotData<T>(index);
    }

    template <typename T> static T GetAotData(Engine::Session& s, RefIdentifier<Image::FieldReference> index)
    {
        return s.Decoder().GetAotData<T>(index);
    }

    template <typename T> static Decode::RefSequence<T> Resolve(Engine::Session& s, Image::RefSequence<T> seq)
    {
        return s.Decoder().Resolve(seq);
    }

    template <typename T> static Decode::OffsetSequence<T> Resolve(Engine::Session& s, Image::OffsetSequence<T> seq)
    {
        return s.Decoder().Resolve(seq);
    }
};

} // namespace Image
