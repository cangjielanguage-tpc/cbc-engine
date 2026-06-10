#pragma once

#include "runtimesupport/runtime.h"
#include "utils/assertion.h"

namespace Interpretation {

class ImplicitException {
public:
    enum class Type {
        NoneValueException,
        ArithmeticException,
        // TODO: support others
    };

    constexpr ImplicitException(Type type) : type(type) {}

    constexpr operator Type() const { return type; }

    const char* GetTypeName() const
    {
        switch (type) {
            case Type::NoneValueException:  return "std.core:NoneValueException";
            case Type::ArithmeticException: return "std.core:ArithmeticException";
            default:                        FATAL("Unknown exception type"); return nullptr;
        }
    }

private:
    Type type;
};

} // namespace Interpretation
