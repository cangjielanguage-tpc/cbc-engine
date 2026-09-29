#pragma once

#include "utils/vector.h"

#include <assert.h>
#include <cstddef>
#include <stdlib.h>
#include <type_traits>

namespace Utils {

/// A non-owning view over a contiguous sequence of elements.
/// Like std::span, but minimal: a pointer and a size.
/// Trivially copyable, no allocations.
template <typename T>
class Span {
    T* ptr;
    size_t count;

public:
    constexpr Span() = default;
    constexpr Span(T* data, size_t size) : ptr(data), count(size) {}

    /// Wraps a const Utils::Vector. Only valid for `Span<const T>`.
    template <typename U>
    Span(const Vector<U>& vec) : ptr(vec.Data()), count(vec.Size())
    {
        static_assert(std::is_const_v<T>, "Span<const T> required to wrap a vector");
    }

    /// Converts Span<U> to Span<T>. Valid when T is const U, or U == T (copy).
    template <typename U>
    Span(const Span<U>& other) : ptr(other.Data()), count(other.Size())
    {
        static_assert(
            std::is_same_v<U, T> || (std::is_const_v<T> && std::is_same_v<std::remove_const_t<T>, U>),
            "Invalid span conversion");
    }

    constexpr T* Data() const { return ptr; }
    constexpr size_t Size() const { return count; }
    constexpr bool Empty() const { return count == 0; }

    constexpr T& operator[](size_t index) const { return ptr[index]; }
    constexpr T& Front() const { return ptr[0]; }
    constexpr T& Back() const { return ptr[count - 1]; }

    // No-exception bounds-checked access
    T& At(size_t index) const
    {
        if (index >= count) {
            assert(false && "Span::At index out of bounds");
            ::abort(); // Immediate termination instead of std::out_of_range exception
        }
        return ptr[index];
    }

    constexpr T* begin() const { return ptr; }
    constexpr T* end() const { return ptr + count; }

    constexpr operator Span<T const>() const { return Span<T const>(ptr, count); }
};

template <typename T> Span(const Vector<T>&) -> Span<const T>;

} // namespace Utils
