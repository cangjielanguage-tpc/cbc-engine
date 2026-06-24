#pragma once

#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

namespace Interpretation {

class ImplicitException {
public:
    enum class Type {
        NoneValueException,
        ArithmeticException,
        // TODO: add other types when needed
    };

    constexpr ImplicitException(Type type) : type(type) {}

    constexpr operator Type() const { return type; }

    static void RegisterExceptionThrower();

private:
    Type type;
};

} // namespace Interpretation
