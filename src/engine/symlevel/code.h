#pragma once

#include "reader.h"
#include "offset.h"


namespace Symlevel {

class Code {
public:

    ~Code()
    {
        delete[] code;
    }

    static Code* Parse(IO::FileId fileID, IO::StreamFileReader& reader);

private:
    Code(IO::FileId fileId, uint32_t methodIdx, uint32_t codeSize, char* code): fileId(fileId), methodIdx(methodIdx), codeSize(codeSize), code(code) {}

    IO::FileId fileId;

    uint32_t methodIdx;
    uint32_t codeSize;
    char* code;
};

} // namespace Symlevel
