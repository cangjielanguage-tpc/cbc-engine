#pragma once

#include "engine/decode/decoder.h"
#include "engine/engine.h"
#include "engine/symlevel/code.h"
#include "engine/symlevel/sequence.h"
#include "io/file_id.h"
#include "offset.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset);
    template <typename T> static T Read(Engine::Session& session, Engine::Identifier<T> id);

    static std::vector<ExceptionRegion> GetExceptionRegions(Engine::Session& session, Code const& code);
    static std::vector<LivenessInfo> GetLivenessInfo(Engine::Session& session, Code const& code);
    static std::vector<StackPtrsInfo> GetStackPtrsInfo(Engine::Session& session, Code const& code);

    template <typename T> static RefSequence<T> ReadRefSeq(IO::StreamFileReader& reader, IO::FileId file)
    {
        auto size     = reader.ReadULEB();
        auto startPos = reader.Position();
        auto endPos   = startPos + size;
        reader.Advance(size);
        return RefSequence<T>(file, startPos, endPos);
    }

    template <typename T> static OffsetSequence<T> ReadOffsSeq(IO::StreamFileReader& reader, IO::FileId file)
    {
        auto size     = reader.ReadULEB();
        auto startPos = reader.Position();
        auto endPos   = startPos + size;
        reader.Advance(size);
        return OffsetSequence<T>(file, startPos, endPos);
    }
};

} // namespace Symlevel
