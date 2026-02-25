#include "reader.h"
#include "code.h"

namespace Symlevel {

Code Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetCodeOffs(offset)
    ); // TODO: use file to access code section offset.

    auto codeSize  = reader.ReadU32();
    auto bytecode  = static_cast<uint8_t*>(session.Allocator().do_allocate(codeSize, alignof(uint8_t)));

    reader.Read(bytecode, codeSize);
    return Code {
        .codePtr  = bytecode,
        .codeSize = codeSize,
    };
}

String Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(
        *session.FileOf(fileId), session.CbcFileOf(fileId).GetStringOffs(offset)
    ); // TODO: use file to access code section offset.

    uint32_t size = reader.ReadULEB();
    auto mem      = static_cast<char*>(session.Allocator().do_allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

} // namespace Symlevel
