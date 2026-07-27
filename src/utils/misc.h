#pragma once

#include "ostream.h"

#include "utils/vector.h"
#include <string>

namespace Std {
namespace Vector {

template <typename T> void Print(Stream::Output& out, const Utils::Vector<T>& vec, const std::string& delim = ", ")
{
    const std::string empty = "";
    const std::string* sep  = &empty;

    out << "[";
    for (const auto& elem : vec) {
        out << *sep << elem;
        sep = &delim;
    }
    out << "]";
}

} // namespace Vector
} // namespace Std
