#pragma once

#include "code.h"
#include "engine/engine.h"
#include "index.h"
#include "io/file_id.h"
#include "io/random_access_file.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include "string.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset)
    {
        return T::Parse(session, fileId, offset);
    }

    template <typename T>
    static std::optional<T> ReadAndResolve(Engine::Session& session, IO::FileId fileId, Offset<T> offset)
    {
        return T::ParseAndResolve(session, fileId, offset);
    }

    template <typename Def> static String ReadName(Engine::Session& session, IO::FileId fileId, Offset<Def> offset)
    {
        return Def::ParseName(session, fileId, offset);
    }

    template <typename Ref, typename Data>
    static Index<Ref> ReadIndex(Engine::Session& session, IO::FileId fileId, Offset<Data> offset)
    {
        return Data::ParseIndex(session, fileId, offset);
    }
};

} // namespace Symlevel
