#include "type_definition.h"


namespace Symlevel {

TypeDefinition TypeDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::ParseOffset(reader);
    auto idx = reader.ReadU32();
    auto tk = reader.ReadU32();

    auto superCount = reader.ReadU32();

    std::vector<uint32_t> supers;
    supers.reserve(superCount);

    char* supersRaw = reinterpret_cast<char*>(supers.data());
    reader.Read(supersRaw, superCount * sizeof(uint32_t));

    return TypeDefinition(
        fileId,
        name,
        idx,
        TypeKind(static_cast<TypeKind::Value>(tk)),
        std::move(supers)
    );
}

} // namespace Symlevel
