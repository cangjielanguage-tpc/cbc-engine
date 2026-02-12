#pragma once

#include "engine/session.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "string.h"
#include "offset.h"
#include "reader.h"


namespace Symlevel {

class Term {
public:
    static Term* Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    const String* GetTermStr() const { return termStr; }
    uint32_t GetIdx() const { return idx; }

private:
    Term(IO::FileId fileId, String* termStr, uint32_t idx): fileId(fileId), termStr(termStr), idx(idx) {}

    const IO::FileId fileId;

    const String* termStr;
    const uint32_t idx;
};

} // namespace Symlevel
