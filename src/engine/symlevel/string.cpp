#include "string.h"


namespace Symlevel {

Offset<String> String::ParseOffset(IO::StreamFileReader &reader)
{
    uint32_t length = reader.ReadULEB();
    reader.Advance(length);
    return length;
}


} // namespace Symlevel
