public func throwImplicitException(tpe: Int64): Int64 {
    match (tpe) {
        case 0 => throw NoneValueException()
        case 1 => throw ArithmeticException()
        case _ => throw Exception("Unknown implicit exception type")
    }
    return tpe
}
