#pragma once

#include "offset.h"
#include "string.h"
#include "io/file_id.h"
#include "engine/engine.h"
#include "code.h"

namespace Symlevel {

class MethodDefinition {
public:
    static MethodDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const Offset<String> Name()  const { return name; }
    inline uint32_t GetIdx()     const { return idx;     }
    inline uint32_t GetSigIdx()  const { return sigIdx;  }
    inline uint32_t GetDeclIdx() const { return declIdx; }
    inline Offset<Code> GetCodeOffs() const { return codeOffs; }

    inline IO::FileId FileId() const { return fileId; }

private:
    MethodDefinition(IO::FileId fileId, Offset<String> name, uint32_t idx, uint32_t sigIdx, uint32_t declIdx, Offset<Code> codeOffs):
        fileId(fileId), name(name), idx(idx), sigIdx(sigIdx), declIdx(declIdx), codeOffs(codeOffs)
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t idx;
    const uint32_t sigIdx;
    const uint32_t declIdx;
    const Offset<Code> codeOffs;
};

} // namespace Symlevel
