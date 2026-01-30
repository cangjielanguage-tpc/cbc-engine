#pragma once

#include "symlevel/io/random_access_file.h"
#include "symlevel/io/file_id.h"

namespace Contexts {

class Session {
public:
    IO::RandomAccessFile* FileOf(IO::FileId fileId);

};

} // namespace Contexts
