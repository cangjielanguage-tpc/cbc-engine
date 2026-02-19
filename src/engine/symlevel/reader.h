#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "io/random_access_file.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include "code.h"
#include "string.h"

namespace Symlevel {

class Reader {
public:

    template <typename T>
    static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset)
    {
        IO::StreamFileReader reader(*session.FileOf(fileId), offset.value);
        return T::Parse(fileId, reader);
    }

    static Code Read(Engine::Session& session, IO::FileId fileId, Offset<Code> offset);
    static String Read(Engine::Session& session, IO::FileId fileId, Offset<String> offset);
};

} // namespace Symlevel
