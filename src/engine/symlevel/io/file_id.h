#pragma once

#include <cstdint>

namespace IO {

struct FileId {
public:
    const int id;

    FileId(int id) : id(id) {}

    operator std::size_t() const { return static_cast<std::size_t>(id); }
};

} // namespace IO
