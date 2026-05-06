#pragma once
#include <optional>

namespace Iterators {

struct DefaultSentinel {};

/// Wrapper around () -> std::optional<T> generator,
/// that allows iterator-like behaviour needed for ranged loops.
template <typename Generator>
class MinimalIterator {
private:
    Generator generator;
    using T = typename decltype(generator())::value_type;
    std::optional<T> current;

    void advance() {
        current = generator();
    }

public:
    explicit MinimalIterator(Generator func) : generator(std::move(func)) {
        advance(); // Fetch first item
    }

    T operator*() const {
        return *current;
    }

    MinimalIterator& operator++() {
        advance();
        return *this;
    }

    bool operator!=(DefaultSentinel) const {
        return current.has_value();
    }
};

// 3. The Universal Wrapper
template <typename GeneratorFunc>
struct SimpleRange {
    GeneratorFunc func;

    auto begin() { return MinimalIterator<GeneratorFunc>(func); }
    auto end()   { return DefaultSentinel{}; }
};

template <typename GeneratorFunc>
SimpleRange<GeneratorFunc> make_range(GeneratorFunc func) {
    return {std::move(func)};
}
}
