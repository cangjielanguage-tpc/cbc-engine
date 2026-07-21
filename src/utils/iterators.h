#pragma once
#include <optional>

namespace Iterators {

struct DefaultSentinel {};

/// Wrapper around () -> std::optional<T> generator,
/// that allows iterator-like behaviour needed for ranged loops.
template <typename Generator> class MinimalIterator {
private:
    Generator& generator;
    using T = typename decltype(generator())::value_type;
    std::optional<T> current;

    void Advance() { current = generator(); }

public:
    explicit MinimalIterator(Generator& generator) : generator(generator)
    {
        Advance(); // Fetch first item
    }

    T operator*() const { return *current; }

    MinimalIterator& operator++()
    {
        Advance();
        return *this;
    }

    bool operator!=(DefaultSentinel) const { return current.has_value(); }
};

template <typename Generator> struct SimpleRange {
    Generator generator;

    auto begin() { return MinimalIterator<Generator>(generator); }

    auto end() { return DefaultSentinel {}; }
};

template <typename Generator> SimpleRange<Generator> MakeRange(Generator func) { return { std::move(func) }; }
} // namespace Iterators
