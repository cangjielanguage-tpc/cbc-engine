#include "method_definition.h"

namespace Symlevel {

MethodDefinition MethodDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::ParseOffset(reader);
    auto idx = reader.ReadU32();
    auto sigIdx = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    //uint32_t codeOffs = reader.ReadU32();
    uint32_t codeOffs = 0;

    return MethodDefinition(fileId, name, idx, sigIdx, declIdx, codeOffs);
}

MethodDefinition& MethodDefinition::Resolve(Engine::Session& session, Engine::MethodDefIdentifier identifier)
{
    return *static_cast<MethodDefinition*>(identifier);
}

} // namespace Symlevel
