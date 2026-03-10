#pragma once

#include "engine/engine.h"
#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include <string>

namespace Symlevel {

class String : public std::string_view {
public:
    static String Parse(Engine::Session& session, IO::FileId fileId, Offset<String> offset);

    String(std::string_view view) : std::string_view(std::move(view)) {}
};

}; // namespace Symlevel
