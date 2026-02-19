#include "term_val.h"

namespace Symlevel {

TermValue TermValue::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto termStr = String::ParseOffset(reader);
    auto idx     = reader.ReadU32();
    return TermValue(fileId, termStr, idx);
}

} // namespace Symlevel
