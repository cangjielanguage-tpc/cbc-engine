#pragma once

namespace Interpretation {

// Must match `throwImplicitException`'s match in cbcengine-helper.cj
enum class Type {
    Successful           = 0,
    NoneValueException   = 1,
    ArithmeticException  = 2,
    OverflowException    = 3,
    SpawnException       = 4,
    AddOverflowException = 5,
    SubOverflowException = 6,
    MulOverflowException = 7,
    DivOverflowException = 8,
    PowOverflowException = 9,
    LshOverflowException = 10,
    RshOverflowException = 11,
    AshOverflowException = 12,
    NegOverflowException = 13,
    LshNegativeShift     = 14,
    RshNegativeShift     = 15,
    AshNegativeShift     = 16,

    // Checked numeric cast overflow reasons
    CastOverflowInfOrNaN = 17,
    CastOverflowI8Min    = 18,
    CastOverflowI8Max    = 19,
    CastOverflowU8Min    = 20,
    CastOverflowU8Max    = 21,
    CastOverflowI16Min   = 22,
    CastOverflowI16Max   = 23,
    CastOverflowU16Min   = 24,
    CastOverflowU16Max   = 25,
    CastOverflowI32Min   = 26,
    CastOverflowI32Max   = 27,
    CastOverflowU32Min   = 28,
    CastOverflowU32Max   = 29,
    CastOverflowI64Min   = 30,
    CastOverflowI64Max   = 31,
    CastOverflowU64Min   = 32,
    CastOverflowU64Max   = 33,

    IndexOutOfBoundsException = 34,

    // TODO: add other types when needed
};

} // namespace Interpretation
