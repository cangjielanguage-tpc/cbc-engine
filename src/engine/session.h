#pragma once

#include "symlevel/io/random_access_file.h"
#include "symlevel/io/file_id.h"

#include "arena.h"

namespace Session {

class Session {
public:
    IO::RandomAccessFile* FileOf(IO::FileId fileId);

private:
    Arena arena;
};

} // namespace Contexts
