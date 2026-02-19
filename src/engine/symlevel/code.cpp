#include "code.h"

namespace Symlevel {

Code Code::Parse(IO::FileId fileID, IO::StreamFileReader& reader)
{
    auto methodIdx = reader.ReadU32();
    auto codeSize  = reader.ReadU32();
    auto offset    = reader.Position();
    reader.Advance(codeSize);

    return Code(fileID, methodIdx, codeSize, offset);
}

} // namespace Symlevel
