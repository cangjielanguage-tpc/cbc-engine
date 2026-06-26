#pragma once

namespace Interpretation {

enum class Type {
    NoneValueException,
    ArithmeticException,
    // TODO: add other types when needed
};

void RegisterExceptionThrower();

} // namespace Interpretation
