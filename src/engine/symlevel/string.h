#pragma once

#include <string>
#include "io/stream_file_reader.h"
#include "io/file_id.h"
#include "offset.h"
#include "reader.h"

namespace Symlevel {

class String : public std::string_view {
public:
    static Offset<String> ParseOffset(IO::StreamFileReader &reader);

    String(std::string_view view) : std::string_view(std::move(view)) {}
};

template <>
class Reader<String> {
public:
    static String Read(Engine::Session &session, IO::FileId fileId, Offset<String> offset) {
        IO::RandomAccessFile* raf = session.FileOf(fileId);
        IO::StreamFileReader reader(raf, offset.value);

        uint32_t size = reader.ReadU32();
        auto mem = static_cast<char*>(session.Allocator().do_allocate(size, alignof(char)));
        reader.Read(mem, size);
        return String(std::string_view(mem, size));
    }
};

}; // namespace Symlevel
