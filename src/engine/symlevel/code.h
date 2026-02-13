#pragma once

#include "reader.h"
#include "offset.h"


namespace Symlevel {

class Code {
public:

    static Code Parse(IO::FileId fileID, IO::StreamFileReader& reader);

private:
    Code(IO::FileId fileId, uint32_t methodIdx, uint32_t codeSize, Offset<Code> offset): fileId(fileId), methodIdx(methodIdx), codeSize(codeSize), offset(offset) {}

    IO::FileId fileId;

    uint32_t methodIdx;
    uint32_t codeSize;
    Offset<Code> offset;
};

} // namespace Symlevel
