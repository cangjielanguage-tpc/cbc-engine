
// This method is called through N2C wrapper, so exception is not really thrown here,
// it is set as pending and then execution directly returns to the caller of N2C wrapper (interpreter code).
@C
public func throwImplicitException(tpe: Int64): Int64 {
    match (tpe) {
        case 0 => throw NoneValueException()
        case 1 => throw ArithmeticException()
        case _ => throw Exception("Unknown implicit exception type")
    }
    return tpe
}
