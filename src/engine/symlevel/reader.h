#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "offset.h"
#include "string.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset)
    {
        return T::Parse(session, fileId, offset);
    }

    template <typename T> static T Read(Engine::Session& session, Engine::Identifier<T> id)
    {
        return T::Parse(session, id.GetFileId(), id.GetOffset());
    }

    template <typename Def> static String ReadName(Engine::Session& session, IO::FileId fileId, Offset<Def> offset)
    {
        return Def::ParseName(session, fileId, offset);
    }
};

} // namespace Symlevel
