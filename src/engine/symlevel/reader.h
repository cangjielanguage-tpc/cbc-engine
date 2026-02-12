#pragma once

#include "engine/session.h"
#include "io/file_id.h"
#include "io/random_access_file.h"
#include "io/stream_file_reader.h"
#include "offset.h"


namespace Symlevel {

template <typename T> class Reader {
public:
    static T* Read(Session::Session &session, IO::FileId fileId, Offset<T> offset) {
        IO::RandomAccessFile* raf = session.FileOf(fileId);
        IO::StreamFileReader reader(raf, offset.value);
        return T::Parse(fileId, reader);
    }
};

} // namespace Symlevel
