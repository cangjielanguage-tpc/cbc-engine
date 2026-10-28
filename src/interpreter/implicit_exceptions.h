#pragma once

namespace Interpretation {

enum class Type {
    Successful = 0,
    NoneValueException,
    ArithmeticException,
    OverflowException,
    SpawnException,
    AddOverflowException,
    SubOverflowException,
    MulOverflowException,
    DivOverflowException,
    UaddOverflowException,
    UsubOverflowException,
    UmulOverflowException,
    UpowOverflowException,
    LshOverflowException,
    RshOverflowException,
    AshOverflowException,
    NegOverflowException,
    LshNegativeShift,
    RshNegativeShift,
    AshNegativeShift,

    // TODO: add other types when needed
};

} // namespace Interpretation
