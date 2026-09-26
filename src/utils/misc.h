#pragma once

#include "ostream.h"
#include "span.h"

#include <string>
#include <vector>

#define UNWRAP_OPT(name, expression, handler)                                                                          \
    auto __##name = (expression);                                                                                      \
    if (!__##name.has_value()) {                                                                                       \
        handler();                                                                                                     \
        return;                                                                                                        \
    }                                                                                                                  \
    auto name = __##name.value();

namespace Std {
namespace Vector {

template <typename T> void Print(Stream::Output& out, Utils::Span<T const> vec, const std::string& delim = ", ")
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

template <typename T> void Print(Stream::Output& out, const std::vector<T>& vec, const std::string& delim = ", ")
{
    Print(out, Utils::Span<T const>(vec), delim);
}

template <typename T> void Print(Stream::Output& out, const Utils::Vector<T>& vec, const std::string& delim = ", ")
{
    Print(out, Utils::Span<T const>(vec), delim);
}

} // namespace Vector
} // namespace Std
