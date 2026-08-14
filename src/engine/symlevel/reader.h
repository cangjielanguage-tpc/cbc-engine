#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "offset.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset);
    template <typename T> static T Read(Engine::Session& session, Engine::Identifier<T> id);
};

} // namespace Symlevel
