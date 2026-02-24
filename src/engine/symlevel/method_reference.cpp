#include "method_reference.h"

namespace Symlevel {

MethodReference MethodReference::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto name         = Offset<String>(reader.ReadU32());
    auto refTypeName  = Offset<String>(reader.ReadU32()); // FIXME: use type term
    auto signatureIdx = 0;                                // FIXME: add signature id

    return MethodReference(fileId, name, 0);
}

} // namespace Symlevel
