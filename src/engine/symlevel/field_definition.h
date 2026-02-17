#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "string.h"
#include "offset.h"


namespace Symlevel {

class FieldDefinition {
public:
    static FieldDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const Offset<String> Name()  const { return name; }
    inline uint32_t GetIdx()     const { return idx; }
    inline uint32_t GetDeclIdx() const { return declIdx; }
    inline uint32_t GetTypeIdx() const { return typeIdx; }


private:
    FieldDefinition(IO::FileId fileId, Offset<String> name, uint32_t idx, uint32_t declIdx, uint32_t typeIdx):
        fileId(fileId), name(name), idx(idx), declIdx(declIdx), typeIdx(typeIdx)
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t idx;
    const uint32_t declIdx;
    const uint32_t typeIdx;
};

} // Symlevel
