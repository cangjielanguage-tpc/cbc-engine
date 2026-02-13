#pragma once

#include "random_access_file.h"

namespace IO {

RandomAccessFile* OpenFile(std::filesystem::path path);

} // namespace IO
