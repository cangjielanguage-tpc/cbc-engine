#include "field_definition.h"


namespace Symlevel {

FieldDefinition FieldDefinition::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::ParseOffset(reader);
    auto idx = reader.ReadU32();
    auto declIdx = reader.ReadU32();
    auto typeIdx = reader.ReadU32();

    return FieldDefinition(fileId, name, idx, declIdx, typeIdx);
}

} // namespace Symlevel
