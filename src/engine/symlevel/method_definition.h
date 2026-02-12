#pragma once

#include "offset.h"
#include "string.h"
#include "io/file_id.h"
#include "context/session.h"
#include "reader.h"


namespace Symlevel {

class MethodDefinition {
public:
    static MethodDefinition* Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const String* Name()  const { return name; }
    inline uint32_t GetIdx()     const { return idx;     }
    inline uint32_t GetSigIdx()  const { return sigIdx;  }
    inline uint32_t GetDeclIdx() const { return declIdx; }

private:
    MethodDefinition(IO::FileId fileId, String* name, uint32_t idx, uint32_t sigIdx, uint32_t declIdx):
        fileId(fileId), name(name), idx(idx), sigIdx(sigIdx), declIdx(declIdx)
    {}

    const IO::FileId fileId;

    const String* name;
    const uint32_t idx;
    const uint32_t sigIdx;
    const uint32_t declIdx;
};

} // namespace Symlevel
