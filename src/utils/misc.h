#pragma once

#include "ostream.h"
#include "span.h"

#include <string>
#include <string_view>

#define UNWRAP_OPT(name, expression, handler)                                                                          \
    auto __##name = (expression);                                                                                      \
    if (!__##name.has_value()) {                                                                                       \
        handler();                                                                                                     \
        return;                                                                                                        \
    }                                                                                                                  \
    auto name = __##name.value();

namespace Std {
namespace Vector {

template <typename T, typename Printer>
void Print(Stream::Output& out, Utils::Span<T const> vec, Printer&& printElem, std::string_view delim = ", ")
{
    std::string_view sep = "";
    Printer print        = printElem;

    out << "[";
    for (const auto& elem : vec) {
        printElem(out, elem);
        out << sep;
        sep = delim;
    }
    out << "]";
}

template <typename T> void Print(Stream::Output& out, Utils::Span<T const> vec, std::string_view delim = ", ")
{
    Print(out, vec, [](Stream::Output& out, T const& elem) { out << elem; }, delim);
}

template <typename T, typename Printer>
void Print(Stream::Output& out, const Utils::Vector<T>& vec, Printer&& printElem, std::string_view delim = ", ")
{
    Print(out, Utils::Span(vec), std::move(printElem), delim);
}

template <typename T> void Print(Stream::Output& out, const Utils::Vector<T>& vec, std::string_view delim = ", ")
{
    Print(out, Utils::Span(vec), delim);
}

} // namespace Vector
} // namespace Std
