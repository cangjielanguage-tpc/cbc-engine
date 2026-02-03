#ifndef CBC_ISA_H
#define CBC_ISA_H

#include <cstdint>
#include <array>

#include "utils/assertion.h"

namespace Cbc {
class IReg {
public:
    enum Value : uint8_t {
        IRZ, IR1, IR2, IR3, IR4, IR5, IR6,
        IR7, IR8, IR9, IR10, IR11, IR12, IR13,
    };
    static constexpr Value values[] = {
        IRZ, IR1, IR2, IR3, IR4, IR5, IR6,
        IR7, IR8, IR9, IR10, IR11, IR12, IR13,
    };

    static constexpr int COUNT = 14;

    constexpr IReg(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }

    inline static IReg From(const uint32_t raw) {
        assert(raw < COUNT);
        return IReg(static_cast<Value>(raw));
    }

private:
    Value _value;
};

class FReg {
public:
    enum Value : uint32_t {
        FR0, FR1, FR2, FR3, FR4, FR5, FR6,
        FR7, FR8, FR9, FR10, FR11, FR12, FR13,
        FR14, FR15
    };
    static constexpr Value values[] = {
        FR0, FR1, FR2, FR3, FR4, FR5, FR6,
        FR7, FR8, FR9, FR10, FR11, FR12, FR13,
        FR14, FR15
    };

    static constexpr int COUNT = 16;

    constexpr FReg(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }

    inline static FReg From(const uint32_t raw) {
        assert(raw < COUNT);
        return FReg(static_cast<Value>(raw));
    }

private:
    Value _value;
};

namespace Format {

class Bits;
constexpr Bits mask_bits(uint32_t b);

class Bits {
public:
    constexpr Bits(const uint32_t value) : _value(value) {}
    constexpr uint32_t Raw() const { return _value; }

    constexpr Bits operator|(Bits other) const {
        auto lhs = this->_value;
        auto rhs = other._value;
        ASSERTION((lhs & rhs) == 0, "Expected to be disjoint");
        return lhs | rhs;
    }

    constexpr Bits operator&(Bits other) const {
        auto lhs = this->_value;
        auto rhs = other._value;
        return lhs & rhs;
    }

    constexpr bool operator==(Bits other) const {
        auto lhs = this->_value;
        auto rhs = other._value;
        return lhs == rhs;
    }

    inline static Bits P(Bits bits, uint32_t freeBits) {
        return bits.Shift(freeBits);
    }

    inline static Bits S(Bits bits, uint32_t n) {
        return bits.In(n);
    }

    inline static Bits E(Bits bits, uint32_t n) {
        return bits.Out(n);
    }

    constexpr Bits Out(uint32_t n) const {
        ASSERTION((*this & mask_bits(n)).IsEmpty(), "Low `n` bits are non-empty");
        return *this;
    }

    constexpr Bits In(uint32_t n) const {
        ASSERTION((*this & mask_bits(n)) == *this, "Has bits set outside of low `n` bits");
        return *this;
    }

    constexpr Bits Shift(uint32_t n) const {
        auto res = _value << n;
        ASSERTION(Bits(res >> n) == *this, "Shift overflowed");
        return res;
    }

    constexpr bool IsEmpty() const {
        return _value == 0;
    }

private:
    uint32_t _value;
};

class Sign {
public:
    enum Value : uint32_t {
        SIGNED = 0b0,
        UNSIGNED = 0b1,
    };

    constexpr Sign(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class Common {
public:
    enum Value : uint32_t {
        ADD  = 0b0000,
        SUB  = 0b0001,
        EXT  = SUB,
        MUL  = 0b0010,
        AND  = 0b0011,
        OR   = 0b0100,
        XOR  = 0b0101,
        SDIV = 0b0110,
        MOV  = SDIV,
        SREM = 0b0111,
        MVST = SREM,
        MREF = MVST,
        UDIV = 0b1000,
        UREM = 0b1001,
        LSR  = 0b1010,
        ASR  = 0b1011,
        LSL  = 0b1100,
    };
    static constexpr Value values[] = {
        ADD, SUB, MUL, AND, OR, XOR, SDIV, SREM, UDIV, UREM, LSR, ASR, LSL,
    };

    constexpr Common(const Value raw) : _value(raw) {}
    constexpr Common(const Bits bits) : _value(Value(bits.Raw())) {}
    constexpr Common(const uint32_t bits) : _value(Value(bits)) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

    constexpr bool B2rAllowed() {
        return (_value >> 3u) == 0;
    }

private:
    Value _value;
};


class OP7A {
public:
    enum Value : uint32_t {
        COMMON,
        CHECKED,
        SETIF,
        FLOAT
    };
    constexpr OP7A(const Value raw) : _value(raw) {}
    constexpr OP7A(const Bits bits) : _value(Value(bits.Raw())) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class Width {
public:
    enum Value : uint32_t {
        W8   = 0b00,
        W16  = 0b01,
        W32  = 0b10,
        W64  = 0b11,
    };

    static constexpr Value values[] = {
        W8, W16, W32, W64,
    };

    constexpr Width(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

    constexpr Bits Common() const {
        ASSERTION(_value == W32 || _value == W64, "TODO: format description");
        return _value & 1;
    }

    constexpr Bits FloatCast() const {
        ASSERTION(_value == W16 || _value == W64, "TODO: format description");
        return (_value & 0b10) >> 1;
    }

private:
    Value _value;
};

class CC {
public:
    enum Value : uint32_t {
        EQ     = 0b0000,
        NE     = 0b0001,
        LT     = 0b0010,
        GE     = 0b0011,
        ULT    = 0b0100,
        UGE    = 0b0101,
        REQ    = 0b0110,
        RNE    = 0b0111,
        FEQ    = 0b1000,
        FNE    = 0b1001,
        FLT    = 0b1010,
        FNLT   = 0b1011,
        FGE    = 0b1100,
        FNGE   = 0b1101,
        TESTZ  = 0b1110,
        TESTNZ = 0b1111,
    };

    constexpr CC(const uint32_t raw) : _value((Value) raw) {}
    constexpr CC(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class OPC {
public:
    constexpr OPC(Sign sign, Bits b)
        : bits(Bits(sign).In(1).Shift(4) | b.In(4)) {}

    constexpr OPC(Common op, Bits b)
        : bits(op.ToBits().In(4).Shift(1) | b.In(1)) {}

    constexpr OPC(Common op, Width w)
        : OPC(op, w.Common()) {}

    constexpr OPC(Common op, Sign s)
        : OPC(op, Bits(s)) {}

    constexpr operator Bits() const { return bits; }
    constexpr Bits ToBits() const { return *this; }
private:
    Bits bits;
};

class ImmKind {
public:
    enum Value : uint32_t {
        VALUE = 0b00,
        LITERAL = 0b01,
    };

    constexpr ImmKind(const Value raw) : _value(raw) {}
    constexpr operator Value() const { return _value; }
    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

constexpr Bits mask_bits(uint32_t b) {
    ASSERTION(1 <= b && b <= 32, "Shift overflow");
    return 0xffffffff >> b;
}

namespace B1 {
    constexpr Bits FORMAT_BITS = 0b01001;
    constexpr Bits MASK = FORMAT_BITS.Shift(3);
    constexpr Bits Fmt(Bits bits) {
        return MASK.Out(3) | bits.In(3);
    }
}

namespace B1piN {
    enum Size : uint32_t {
        SZ_8 = 0b00,
        SZ_16 = 0b01,
        SZ_32 = 0b10,
        SZ_48 = 0b11,
    };

    constexpr Bits FORMAT_BITS = 0b01110;

    constexpr Bits Fmt(Size sz, Sign sign) {
        return FORMAT_BITS.In(5).Shift(3) |
                     Bits(sign).In(1).Shift(2) |
                     Bits(sz).In(2);
    }
}

namespace B2rr {
    constexpr Bits FORMAT_BITS = 0b0000;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(4);

    constexpr Bits Fmt(Common op, Width width) {
        ASSERTION(op.B2rAllowed(), "not allowed in B2r format");
        return BYTE_MASK | OPC(op, width).ToBits().In(4);
    }

    constexpr uint32_t Fmt(Common::Value op, Width::Value width) {
        return Fmt(Common(op), Width(width)).Raw();
    }
}

namespace B2hr {
    constexpr Bits FORMAT_BITS = 0b0001;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(4);

    constexpr Bits Fmt(Common op, Bits b) {
        ASSERTION(op.B2rAllowed(), "not allowed in B2r format");
        return BYTE_MASK | OPC(op, b).ToBits().In(4);
    }

    constexpr Bits Fmt(Common op, Width width) {
        return Fmt(op, width.Common());
    }

    constexpr uint32_t Fmt(Common::Value op, Width::Value width) {
        return Fmt(Common(op), Width(width)).Raw();
    }

    constexpr Bits Fmt(Common op, Sign sign) {
        return Fmt(op, sign);
    }
}

namespace B2rrd8 {
    constexpr Bits FORMAT_BITS = 0b0101;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(4);

    constexpr Bits Fmt(CC cc, Width w) {
        return BYTE_MASK | cc.ToBits().In(3).Shift(1) | w.Common();
    }

    constexpr uint32_t Fmt(CC::Value cc, Width::Value w) {
        return Fmt(CC(cc), Width(w)).Raw();
    }
}

namespace SymbolicObjectControl {
    constexpr Bits FORMAT_BITS = 0b1010;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(4);

    constexpr Bits Fmt(uint32_t opc) {
        return BYTE_MASK | Bits(opc).In(4);
    }
}

namespace B2xrI {
    class Opc1011 {
    public:
        enum Value : uint32_t {
            NEWOBJ     = 0b0000,
            NEWOBJ_VST = 0b0001,
            NEWOBJ_R   = 0b0010, // TODO: not needed
        };

        constexpr static Bits OPCODE = SymbolicObjectControl::Fmt(0b1011);

        constexpr Opc1011(const Value raw) : _value(raw) {}
        constexpr operator Value() const { return _value; }
        constexpr Bits ToBits() const { return _value; }

    private:
        Value _value;
    };
}

namespace B3xrrr {
    constexpr Bits FORMAT_BITS = 0b01000;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(3);

    constexpr Bits Fmt(Bits low3Bits) {
        return BYTE_MASK | low3Bits.In(3);
    }

}

namespace B3xrrt4i16 {
    constexpr Bits FORMAT_BITS = 0b001;
    constexpr Bits K_BITS = 0b10; // for i16 imm
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(5) | K_BITS.Shift(3);

    constexpr Bits Fmt(Bits low3Bits) {
        return BYTE_MASK | low3Bits.In(3);
    }
}

namespace B3xrrkI {
    constexpr Bits FORMAT_BITS = 0b00111;
    constexpr Bits BYTE_MASK = FORMAT_BITS.Shift(3);

    constexpr Bits Fmt(Bits low3Bits) {
        return BYTE_MASK | low3Bits.In(3);
    }
}

namespace ExtBrr {
    constexpr uint32_t OPCODE_START = 178;
    constexpr uint32_t INSTRUCTION_SIZE = 4;

    constexpr Bits Fmt(ImmKind immKind, Width width, CC cc) {
        auto bits = width.Common().In(1).Shift(1)
                  | immKind.ToBits().In(1)
                  | cc.ToBits().In(3).Shift(2);
        return Bits(bits.Raw() + OPCODE_START);
    }

    constexpr uint32_t Fmt(ImmKind::Value immKind, Width::Value width, CC::Value cc) {
        return Fmt(ImmKind(immKind), Width(width), CC(cc)).Raw();
    }
};

namespace ExtRet {
    constexpr uint32_t OPCODE = 254;

    constexpr Bits Fmt() {
        return Bits(OPCODE);
    }
};

} // namespace Format
} // namespace Cbc

#endif // CBC_ISA_H
