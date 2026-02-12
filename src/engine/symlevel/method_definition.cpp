#include "method_definition.h"


namespace Symlevel {

MethodDefinition* MethodDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::Parse(fileId, reader);
    auto idx = reader.ReadU32();
    auto sigIdx = reader.ReadU32();
    auto declIdx = reader.ReadU32();

    return new MethodDefinition(fileId, name, idx, sigIdx, declIdx);
}

} // namespace Symlevel
