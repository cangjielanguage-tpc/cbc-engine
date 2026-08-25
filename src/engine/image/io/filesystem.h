#pragma once

#include "random_access_file.h"
#include <optional>

namespace IO {

std::optional<std::unique_ptr<RandomAccessFile>> OpenFile(std::string const& path);

std::optional<std::unique_ptr<RandomAccessFile>> TryOpenFile(std::string const& path);

} // namespace IO
