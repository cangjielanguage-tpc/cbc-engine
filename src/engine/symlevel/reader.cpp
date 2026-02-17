#include "code.h"
#include "reader.h"

namespace Symlevel {

Code Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<Code> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), offset.value); // TODO: use file to access code section offset.

    auto methodIdx = reader.ReadU32();
    auto codeSize = reader.ReadU32();
    auto bytecode = static_cast<char*>(session.Allocator().do_allocate(codeSize, alignof(char)));

    reader.Read(bytecode, codeSize);
    return Code{codeSize, bytecode};
}

String Reader::Read(Engine::Session& session, IO::FileId fileId, Offset<String> offset)
{
    IO::StreamFileReader reader(*session.FileOf(fileId), offset.value); // TODO: use file to access code section offset.

    uint32_t size = reader.ReadU32();
    auto mem = static_cast<char*>(session.Allocator().do_allocate(size, alignof(char)));
    reader.Read(mem, size);
    return String(std::string_view(mem, size));
}

} // namespace Symlevel
