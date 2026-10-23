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
        case 13 => throw OverflowException("lsh")
        case 14 => throw OverflowException("rsh")
        case 15 => throw OverflowException("ash")
        case 16 => throw OverflowException("neg")
        case _ => throw Exception("Unknown implicit exception type")
    }
    return tpe
}
