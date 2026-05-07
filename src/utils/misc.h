#pragma once

#include <string>
#include <vector>

namespace Std {
namespace Vector {

template <typename T>
std::string ToString(const std::vector<T>& vec, const std::string& delim = ", ") {
    std::string result = "[";
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        result += std::to_string(*it);
        result = std::next(it) != vec.end() ? result += delim : result;
    }
    return result + "]";
}

} //namespace Vector
}