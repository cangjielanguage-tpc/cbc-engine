#include "method_reference.h"


namespace Symlevel {

MethodReference MethodReference::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name = String::ParseOffset(reader);
    auto idx = reader.ReadU32();
    auto methodIdx = reader.ReadU32();
    auto refTypeIdx = reader.ReadU32();

    return MethodReference(fileId, name, idx, methodIdx, refTypeIdx);
}

} // namespace Symlevel
