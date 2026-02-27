#pragma once

#include <cstdint>

namespace Symlevel {

template <typename T> struct Index {
    uint32_t region : 8;
    uint32_t index : 24;
};

} // namespace Symlevel
