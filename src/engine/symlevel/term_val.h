#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "string.h"
#include "offset.h"


namespace Symlevel {

class TermValue {
public:
    static TermValue Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    const Offset<String> GetTermStr() const { return termStr; }
    uint32_t GetIdx() const { return idx; }

private:
    TermValue(IO::FileId fileId, Offset<String> termStr, uint32_t idx): fileId(fileId), termStr(termStr), idx(idx) {}

    const IO::FileId fileId;

    const Offset<String> termStr;
    const uint32_t idx;
};

} // namespace Symlevel
