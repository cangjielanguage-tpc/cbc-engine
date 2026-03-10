#include "string.h"

namespace Symlevel {

String String::Parse(Engine::Session& session, IO::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), offset + session.CbcFileOf(fileId).GetStringSectionOffs());

    uint32_t size = reader.ReadULEB();
    auto mem      = static_cast<char*>(session.Allocator().do_allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

} // namespace Symlevel
