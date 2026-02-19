#pragma once

#include "io/file_id.h"
#include "io/stream_file_reader.h"
#include "offset.h"
#include <string>

namespace Symlevel {

class String : public std::string_view {
public:
    static Offset<String> ParseOffset(IO::StreamFileReader& reader);

    String(std::string_view view) : std::string_view(std::move(view)) {}
};

}; // namespace Symlevel
