#include "term_val.h"

namespace Symlevel {

TermValue TermValue::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto termStr = Offset<String>(reader.ReadU32()); // FIXME: wrong format
    auto idx     = reader.ReadU32();
    return TermValue(fileId, termStr, idx);
}

} // namespace Symlevel
