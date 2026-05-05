#include "string.h"

namespace Symlevel {

String String::Parse(Engine::Session& session, IO::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetStringSectionOffs() + offset);

    uint32_t size = reader.ReadULEB();
    auto mem      = static_cast<char*>(session.Allocator().Allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

String String::Parse(Engine::Session& session, Engine::Identifier<String> ident)
{
    return Parse(session, ident.GetFileId(), ident.GetOffset());
}

} // namespace Symlevel
