#pragma once

#include "ostream.h"

#include <string>
#include <vector>

namespace Std {
namespace Vector {

template <typename T> void Print(Stream::Output& out, const std::vector<T>& vec, const std::string& delim = ", ")
{
    std::string empty = "";
    std::string& sep  = empty;
    for (auto& elem : vec) {
        out << sep << elem;
        sep = delim;
    }
}

} //namespace Vector
}