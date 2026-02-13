#pragma once

#include <string>
#include "io/stream_file_reader.h"
#include "io/file_id.h"
#include "offset.h"

namespace Symlevel {

class String : public std::string_view {
public:
    static Offset<String> ParseOffset(IO::StreamFileReader &reader);
    static String Parse(IO::FileId fileId, IO::StreamFileReader &reader);

    String(std::string_view view) : std::string_view(std::move(view)) {}
};

}; // namespace Symlevel
