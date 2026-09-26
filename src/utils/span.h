#pragma once

#include <cstddef>
#include <type_traits>
#include <vector>

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

    /// Wraps a const vector. Only valid for `Span<const T>`.
    template <typename U>
    Span(const std::vector<U>& vec) : ptr(vec.data()), count(vec.size())
    {
        static_assert(std::is_const_v<T>, "Span<const T> required to wrap a vector");
    }

    /// Converts Span<U> to Span<T>. Valid when T is const U, or U == T (copy).
    template <typename U>
    Span(const Span<U>& other) : ptr(other.data()), count(other.size())
    {
        static_assert(
            std::is_same_v<U, T> || (std::is_const_v<T> && std::is_same_v<std::remove_const_t<T>, U>),
            "Invalid span conversion");
    }

    constexpr T* data() const { return ptr; }
    constexpr size_t size() const { return count; }
    constexpr bool empty() const { return count == 0; }

    constexpr T& operator[](size_t index) const { return ptr[index]; }
    constexpr T& front() const { return ptr[0]; }
    constexpr T& back() const { return ptr[count - 1]; }

    constexpr T* begin() const { return ptr; }
    constexpr T* end() const { return ptr + count; }
};

template <typename T> Span(const std::vector<T>&) -> Span<const T>;

} // namespace Utils
