#pragma once

#include "engine/engine.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"
#include "offset.h"
#include "engine/identifiers.h"

#include <cstdint>
#include <vector>

namespace Symlevel {

template <typename T> class OffsetSequence {
public:
    OffsetSequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos)
    {}

    static OffsetSequence<T> Parse(IO::StreamFileReader& reader, IO::FileId id)
    {
        auto size     = reader.ReadULEB();
        auto startPos = reader.Position();
        auto endPos   = startPos + size;
        return OffsetSequence<T>(id, startPos, endPos);
    }

    void Read(Engine::Session& session, std::vector<Engine::Identifier<T>>& offsets) const
    {
        IO::StreamFileReader reader(*session.FileOf(file), startPos);
        while (reader.Position() < endPos) {
            auto offs = Offset<T>(reader.ReadULEB());
            offsets.emplace_back(offs, file);
        }
    }

    IO::FileId FileId() const { return file; }

private:
    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

} // namespace Symlevel
