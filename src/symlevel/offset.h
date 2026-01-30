#pragma once

#include <cstdint>


namespace Symlevel {

template <typename T> struct Offset {

    const std::size_t value;

    Offset(std::size_t value): value(value) {}
};

} // namespace Symlevel
