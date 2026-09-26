#pragma once

#include <utility>

namespace Utils {

// Non-allocating callable wrapper. Stores a pointer to the wrapped callable
// and a trampoline function pointer. The wrapped callable must outlive the
// Function.
template <typename Signature>
class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
    void* object;
    R (*invoke)(void*, Args...);

public:
    template <typename F, typename = decltype(std::declval<F&>()(std::declval<Args>()...))>
    Function(F& f)
        : object(&f)
        , invoke([](void* obj, Args... args) -> R {
              auto* fn = static_cast<F*>(obj);
              return (*fn)(args...);
            })
    {}

    R operator()(Args... args) const { return invoke(object, args...); }
};

} // namespace Utils
