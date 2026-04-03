#pragma once

#include <cstdint>

namespace Symlevel {

template <typename T> union Index {
    struct {
        uint32_t region : 8;
        uint32_t index : 24;
    };

    uint32_t raw;
};

} // namespace Symlevel
