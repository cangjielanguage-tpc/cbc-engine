#pragma once

#include <string>
#include "io/stream_file_reader.h"
#include "io/file_id.h"


namespace Symlevel {

class String {
public:
    static String* Parse(IO::FileId fileId, IO::StreamFileReader &reader);

private:
    String(IO::FileId fileId, std::string content) : fileId(fileId), content(std::move(content)) {}

    IO::FileId fileId;
    std::string content;
};

}; // namespace Symlevel
