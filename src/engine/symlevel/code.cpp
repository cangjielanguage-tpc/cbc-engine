#include "code.h"


namespace Symlevel {

Code* Code::Parse(IO::FileId fileID, IO::StreamFileReader& reader)
{
    auto methodIdx = reader.ReadU32();
    auto codeSize = reader.ReadU32();

    char* code = new char[codeSize];
    reader.Read(code, codeSize);

    return new Code(fileID, methodIdx, codeSize, code);
}

} // namespace Symlevel
