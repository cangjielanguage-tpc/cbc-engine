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
        case 9 => throw OverflowException("uadd")
        case 10 => throw OverflowException("usub")
        case 11 => throw OverflowException("umul")
        case 12 => throw OverflowException("pow")
        case 13 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 14 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 15 => throw ArithmeticException("Overshift: Value of right operand is greater than or equal to the width of left operand!")
        case 16 => throw OverflowException("neg")
        case 17 => throw ArithmeticException("Overshift: Negative shift count!")
        case 18 => throw ArithmeticException("Overshift: Negative shift count!")
        case 19 => throw ArithmeticException("Overshift: Negative shift count!")
        case _ => throw Exception("Unknown implicit exception type")
    }
    return tpe
}
