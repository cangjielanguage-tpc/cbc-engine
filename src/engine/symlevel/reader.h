#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "offset.h"
#include "string.h"

namespace Symlevel {

class Reader {
public:
    template <typename T> static T Read(Engine::Session& session, IO::FileId fileId, Offset<T> offset);
    template <typename T> static T Read(Engine::Session& session, Engine::Identifier<T> id);
};

template <> Symlevel::String Reader::Read<Symlevel::String>(Engine::Session& session, Engine::Identifier<String> id);
template <>
Symlevel::String Reader::Read<Symlevel::String>(
    Engine::Session& session, IO::FileId fileId, Offset<Symlevel::String> offset
);

} // namespace Symlevel
