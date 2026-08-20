#include "string.h"
#include "reader.h"

namespace Image {

template <> String Reader::Read(Engine::Session& session, Image::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), session.CbcFileOf(fileId).GetStringSectionOffs() + offset);

    uint32_t size = reader.ReadULEB();
    auto mem      = static_cast<char*>(session.Allocator().Allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

template <> String Reader::Read(Engine::Session& session, Image::Identifier<String> ident)
{
    return Reader::Read(session, ident.GetFileId(), ident.GetOffset());
}

} // namespace Image
