package cangjie.runtime.cbcengine.helper

public func throwImplicitException(tpe: Int64): Int64 {
    match (tpe) {
        case 1 => throw NoneValueException()
        case 2 => throw ArithmeticException()
        case 3 => throw OverflowException()
        case 4 => throw SpawnException()
        case 5 => throw OverflowException("add")
        case 6 => throw OverflowException("sub")
        case 7 => throw OverflowException("mul")
        case 8 => throw OverflowException("div")
        case 9 => throw OverflowException("pow")
        case 10 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 11 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 12 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 13 => throw OverflowException("neg")
        case 14 => throw ArithmeticException("Overshift: Negative shift count!")
        case 15 => throw ArithmeticException("Overshift: Negative shift count!")
        case 16 => throw ArithmeticException("Overshift: Negative shift count!")
        // Checked numeric cast overflow reasons; the order must match
        // `src/interpreter/implicit_exceptions.h`.
        case 17 => throw OverflowException("Casting Infinite or NaN value to integer.")
        case 18 => throw OverflowException("The result would be less than Int8.Min.")
        case 19 => throw OverflowException("The result would be greater than Int8.Max.")
        case 20 => throw OverflowException("The result would be less than UInt8.Min.")
        case 21 => throw OverflowException("The result would be greater than UInt8.Max.")
        case 22 => throw OverflowException("The result would be less than Int16.Min.")
        case 23 => throw OverflowException("The result would be greater than Int16.Max.")
        case 24 => throw OverflowException("The result would be less than UInt16.Min.")
        case 25 => throw OverflowException("The result would be greater than UInt16.Max.")
        case 26 => throw OverflowException("The result would be less than Int32.Min.")
        case 27 => throw OverflowException("The result would be greater than Int32.Max.")
        case 28 => throw OverflowException("The result would be less than UInt32.Min.")
        case 29 => throw OverflowException("The result would be greater than UInt32.Max.")
        case 30 => throw OverflowException("The result would be less than Int64.Min.")
        case 31 => throw OverflowException("The result would be greater than Int64.Max.")
        case 32 => throw OverflowException("The result would be less than UInt64.Min.")
        case 33 => throw OverflowException("The result would be greater than UInt64.Max.")
        case _ => throw Exception("Unknown implicit exception type")
    }
    return tpe
}
