#pragma once

#include "ostream.h"

#include <string>
#include <vector>

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
