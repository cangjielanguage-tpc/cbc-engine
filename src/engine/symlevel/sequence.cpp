#include "sequence.h"
#include "engine/engine.h"
#include "engine/symlevel/io/file_id.h"
#include "engine/symlevel/io/stream_file_reader.h"

namespace Symlevel {

Sequence::Sequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos) {}

Sequence Sequence::Empty() { return Sequence(IO::FileId(0), 0, 0); }

Sequence Sequence::Parse(IO::StreamFileReader& reader, IO::FileId id)
{
    auto size     = reader.ReadULEB();
    auto startPos = reader.Position();
    auto endPos   = startPos + size;
    reader.Advance(size);
    return Sequence { id, startPos, endPos };
}

Sequence::Generator Sequence::Values(Engine::Session& session) const
{
    auto [_, raf] = session.File(file);
    return Generator {
        .reader = IO::StreamFileReader(raf, startPos),
        .seq = *this,
    };
}

IO::FileId Sequence::FileId() const
{
    return file;
}

std::optional<uint32_t> Sequence::Generator::operator()()
{
    if (reader.Position() < seq.endPos) {
        return reader.ReadULEB();
    }
    return std::nullopt;
}

} // namespace Symlevel
