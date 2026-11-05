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
    PowOverflowException,
    LshOverflowException,
    RshOverflowException,
    AshOverflowException,
    NegOverflowException,
    LshNegativeShift,
    RshNegativeShift,
    AshNegativeShift,

    // Checked numeric cast overflow reasons. Must match
    // `tools/launcher/cbcengine-helper.cj`.
    CastOverflowInfOrNaN,
    CastOverflowI8Min,
    CastOverflowI8Max,
    CastOverflowU8Min,
    CastOverflowU8Max,
    CastOverflowI16Min,
    CastOverflowI16Max,
    CastOverflowU16Min,
    CastOverflowU16Max,
    CastOverflowI32Min,
    CastOverflowI32Max,
    CastOverflowU32Min,
    CastOverflowU32Max,
    CastOverflowI64Min,
    CastOverflowI64Max,
    CastOverflowU64Min,
    CastOverflowU64Max,

    // TODO: add other types when needed
};

} // namespace Interpretation
