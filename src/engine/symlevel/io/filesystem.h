#pragma once

#include "random_access_file.h"

namespace IO {

std::unique_ptr<RandomAccessFile> OpenFile(std::filesystem::path path);

} // namespace IO
