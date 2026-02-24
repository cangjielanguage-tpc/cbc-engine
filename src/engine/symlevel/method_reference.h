#pragma once

#include "io/file_id.h"
#include "offset.h"
#include "reader.h"
#include "string.h"

namespace Symlevel {

class MethodReference {
public:
    static MethodReference Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const Offset<String> Name() const { return name; }
    inline uint32_t GetRefTypeIdx() const { return refTypeIdx; }

private:
    MethodReference(IO::FileId fileId, Offset<String> name, uint32_t refTypeIdx)
        : fileId(fileId),
          name(name),
          refTypeIdx(refTypeIdx)
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t refTypeIdx;
};

} // namespace Symlevel
