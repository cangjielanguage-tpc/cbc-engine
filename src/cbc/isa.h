#pragma once

#include <cstdint>
#include <string_view>

#include "cbc/decoder.h"
#include "isa_opcodes.h"
#include "utils/assertion.h"

namespace Cbc {

class Opcode {
public:
#define DECLARE_OPCODE(opc, func) opc,
#define OPCODE_STR(opc, func)                                                                                          \
    case opc: return std::string_view(#opc);

    enum Value : uint8_t {
        ISA_OPCODES(DECLARE_OPCODE)
    };

    constexpr Opcode(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr std::string_view ToStr()
    {
        switch (_value) {
            ISA_OPCODES(OPCODE_STR);
        }
        return std::string_view("<invalid>");
    }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

class RegSymGroup {
public:
#define DECLARE_OPCODE(opc) opc,
#define OPCODE_STR(opc)                                                                                                \
    case opc: return std::string_view(#opc);

    enum Value : uint8_t {
        ISA_REG_SYM_GROUP_OPCODES(DECLARE_OPCODE) LAST = CallInterf
    };

    constexpr RegSymGroup(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr static RegSymGroup From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr std::string_view ToStr()
    {
        switch (_value) {
            ISA_REG_SYM_GROUP_OPCODES(OPCODE_STR);
        }
        return std::string_view("<invalid>");
    }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

class RegGroup {
public:
#define DECLARE_OPCODE(opc) opc,
#define OPCODE_STR(opc)                                                                                                \
    case opc: return std::string_view(#opc);

    enum Value : uint8_t {
        ISA_REG_GROUP_OPCODES(DECLARE_OPCODE) LAST = Throw
    };

    constexpr RegGroup(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr static RegGroup From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr std::string_view ToStr()
    {
        switch (_value) {
            ISA_REG_GROUP_OPCODES(OPCODE_STR);
        }
        return std::string_view("<invalid>");
    }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

#undef ISA_OPCODES

typedef uint8_t Opcode_t;

enum class InputOpcode : Opcode_t {
    Bcc32Eq,
    Bcc32Ne,
    Bcc32Lt,
    Bcc32Ult,
    Bcc32Uge,
    Bcc32Req,
    Bcc64Eq,
    Bcc64Ne,
    Bcc64Lt,
    Bcc64Ge,
    Bcc64Ult,
    Bcc64Req,
    Bcc64Rne,
    Bcc32,
    Bcc64,
    BccImm32,
    BccImm64,
    Jump,
    WidePrefix,
    Mov32,
    Mov64,
    Mov32i,
    Mov64i,
    MovRef,
    FMov32,
    FMov64,
    Mov32_FloatToInt,
    Mov64_FloatToInt,
    Mov32_IntToFloat,
    Mov64_IntToFloat,
    Add32,
    Sub32,
    Mul32,
    And32,
    Or32,
    Xor32,
    UDiv32,
    URem32,
    LSR32,
    ASR32,
    LSL32,
    Add64,
    Sub64,
    Mul64,
    And64,
    Or64,
    Xor64,
    UDiv64,
    URem64,
    LSR64,
    ASR64,
    LSL64,
    Binary32,
    Binary64,
    BinaryImm32,
    BinaryImm64,
    Cast,
    NewArr,
    GcPoint,
    PrepareRecord,
    CallInterf,
    ZeroRefs,
    Scc32,
    Scc64,
    SccImm32,
    SccImm64,
    InstanceOff,
    LoadTypeInfoObj,
    RegSymGroup,
    RegGroup,
    InitObj,
    InitString,
    ArrayLength,
    ArrayIndexCheck,
    FBinary32,
    FBinary64,
    FBinaryImm32,
    FBinaryImm64,
    ___LAST,
};

enum class InputCommonOpc : Opcode_t {
    Add,
    Sub,
    Mul,
    And,
    Or,
    Xor,
    DivSigned,
    RemSigned,
    DivUnsigned,
    RemUnsigned,
    Lsr,
    Asr,
    Lsl,
    ___LAST
};

enum class InputCheckedOpc : Opcode_t {
    Add,
    Sub,
    Mul,
    Div,
    ___LAST
};

enum class InputFloatOpc : Opcode_t {
    Add,
    Sub,
    Mul,
    Div,
    Mov,
    Neg,
    Abs,
    Sqrt,
    ___LAST
};

enum class InputCcOpc : Opcode_t {
    EQ,
    NE,
    LT,
    GE,
    ULT,
    UGE,
    REQ,
    RNE,
    FEQ,
    FNE,
    FLT,
    FNLT,
    FGE,
    FNGE,
    TESTZ,
    TESTNZ
};

constexpr Opcode_t Opc(const InputOpcode opc) { return static_cast<Opcode_t>(opc); }
constexpr Opcode_t Opc(const InputCommonOpc opc) { return static_cast<Opcode_t>(opc); }
constexpr Opcode_t Opc(const InputCheckedOpc opc) { return static_cast<Opcode_t>(opc); }

constexpr Opcode_t Opc(const InputFloatOpc opc) { return static_cast<Opcode_t>(opc); }
constexpr Opcode_t Opc(const InputCcOpc opc) { return static_cast<Opcode_t>(opc); }

class IReg {
public:
#define IREG_VALUES(X)                                                                                                 \
    X(IRZ)                                                                                                             \
    X(IR1)                                                                                                             \
    X(IR2)                                                                                                             \
    X(IR3)                                                                                                             \
    X(IR4)                                                                                                             \
    X(IR5)                                                                                                             \
    X(IR6)                                                                                                             \
    X(IR7)                                                                                                             \
    X(IR8)                                                                                                             \
    X(IR9)                                                                                                             \
    X(IR10)                                                                                                            \
    X(IR11)                                                                                                            \
    X(IR12)                                                                                                            \
    X(IR13)

#define IREG_ENUM(opc) opc,

    enum Value : uint8_t {
        IREG_VALUES(IREG_ENUM)
    };

    static constexpr int COUNT = 14;

    constexpr IReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

#define IREG_TO_STR(opc)                                                                                               \
    case opc: return #opc;

    constexpr std::string_view ToStr() const
    {
        switch (_value) {
            IREG_VALUES(IREG_TO_STR)
        }
        return "<invalid>";
    }

    inline static IReg From(const uint32_t raw)
    {
        ASSERT(raw < COUNT);
        return IReg(static_cast<Value>(raw));
    }

#undef IREG_TO_STR
#undef IREG_VALUES

private:
    Value _value;
};

class FReg {
public:
#define FREG_VALUES(X)                                                                                                 \
    X(FR0)                                                                                                             \
    X(FR1)                                                                                                             \
    X(FR2)                                                                                                             \
    X(FR3)                                                                                                             \
    X(FR4)                                                                                                             \
    X(FR5)                                                                                                             \
    X(FR6)                                                                                                             \
    X(FR7)                                                                                                             \
    X(FR8)                                                                                                             \
    X(FR9)                                                                                                             \
    X(FR10)                                                                                                            \
    X(FR11)                                                                                                            \
    X(FR12)                                                                                                            \
    X(FR13)                                                                                                            \
    X(FR14)                                                                                                            \
    X(FR15)

#define FREG_ENUM(opc) opc,
    enum Value : uint32_t {
        FREG_VALUES(FREG_ENUM)
    };

    static constexpr int COUNT = 16;

    constexpr FReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

#define FREG_TO_STR(opc)                                                                                               \
    case opc: return #opc;

    constexpr std::string_view ToStr() const
    {
        switch (_value) {
            FREG_VALUES(FREG_TO_STR)
        }
        return "<invalid>";
    }

    inline static FReg From(const uint32_t raw)
    {
        ASSERT(raw < COUNT);
        return FReg(static_cast<Value>(raw));
    }

#undef FREG_TO_STR
#undef FREG_VALUES

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

    constexpr Bits operator|(Bits other) const
    {
        auto lhs = this->_value;
        auto rhs = other._value;
        ASSERTION((lhs & rhs) == 0, "Expected to be disjoint");
        return lhs | rhs;
    }

    constexpr Bits operator&(Bits other) const
    {
        auto lhs = this->_value;
        auto rhs = other._value;
        return lhs & rhs;
    }

    constexpr bool operator==(Bits other) const
    {
        auto lhs = this->_value;
        auto rhs = other._value;
        return lhs == rhs;
    }

    inline static Bits P(Bits bits, uint32_t freeBits) { return bits.Shift(freeBits); }

    inline static Bits S(Bits bits, uint32_t n) { return bits.In(n); }

    inline static Bits E(Bits bits, uint32_t n) { return bits.Out(n); }

    constexpr Bits Out(uint32_t n) const
    {
        ASSERTION((*this & mask_bits(n)).IsEmpty(), "Low `n` bits are non-empty");
        return *this;
    }

    constexpr Bits In(uint32_t n) const
    {
        ASSERTION((*this & mask_bits(n)) == *this, "Has bits set outside of low `n` bits");
        return *this;
    }

    constexpr Bits Shift(uint32_t n) const
    {
        auto res = _value << n;
        ASSERTION(Bits(res >> n) == *this, "Shift overflowed");
        return res;
    }

    constexpr bool IsEmpty() const { return _value == 0; }

private:
    uint32_t _value;
};

class Sign {
public:
    enum Value : uint32_t {
        SIGNED   = 0b0,
        UNSIGNED = 0b1,
    };

    constexpr Sign(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class CbcTypeKind {
public:
    enum Value : uint32_t {
        INVALID = 0x00,
        VOID    = 0x01,
        U1      = 0x02,
        I8      = 0x03,
        U8      = 0x04,
        CHAR    = 0x05,
        I32     = 0x06,
        U32     = 0x07,
        F32     = 0x08,
        F64     = 0x09,
        I64     = 0x0a,
        U64     = 0x0b,
        NNREF   = 0x0c, // non-nullable ref
        REF     = 0x0d,
        REC     = 0x0e,
        I16     = 0x0f,
        U16     = 0x10,
        F16     = 0x11,
        IN      = 0x12, // int native
        UN      = 0x13, // uint native
        VA      = 0x14, // varray
        TTI     = 0x15, // ThisTypeInfo
    };

    constexpr CbcTypeKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class Common {
public:
#define CommonValue(X)                                                                                                 \
    X(ADD, 0b0000, "add")                                                                                              \
    X(SUB, 0b0001, "sub")                                                                                              \
    X(MUL, 0b0010, "mul")                                                                                              \
    X(AND, 0b0011, "and")                                                                                              \
    X(OR, 0b0100, "or")                                                                                                \
    X(XOR, 0b0101, "xor")                                                                                              \
    X(SDIV, 0b0110, "sdiv")                                                                                            \
    X(SREM, 0b0111, "srem")                                                                                            \
    X(UDIV, 0b1000, "udiv")                                                                                            \
    X(UREM, 0b1001, "urem")                                                                                            \
    X(LSR, 0b1010, "lsr")                                                                                              \
    X(ASR, 0b1011, "asr")                                                                                              \
    X(LSL, 0b1100, "lsl")

#define CommonEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        CommonValue(CommonEnum) LAST = LSL
    };

#undef CommonEnum

    static constexpr auto EXT  = SUB;
    static constexpr auto MOV  = SDIV;
    static constexpr auto MVST = SREM;
    static constexpr auto MREF = MVST;

    static constexpr Value values[] = {
        ADD, SUB, MUL, AND, OR, XOR, SDIV, SREM, UDIV, UREM, LSR, ASR, LSL,
    };

    constexpr Common(const Value raw) : _value(raw) {}

    constexpr Common(const Bits bits) : _value(Value(bits.Raw())) {}

    constexpr operator Value() const { return _value; }

    constexpr static Common From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr Bits ToBits() const { return _value; }

    constexpr std::string_view ToStr()
    {
#define CommonStr(opc, value, str)                                                                                     \
    case opc: return std::string_view(str);
        switch (_value) {
            CommonValue(CommonStr);
        }
        return std::string_view("<invalid>");
#undef CommonStr
    }

private:
    Value _value;
};

class FloatOperations {
public:
#define FloatOperationsValue(X)                                                                                        \
    X(FADD, 0b0000, "fadd")                                                                                            \
    X(FSUB, 0b0001, "fsub")                                                                                            \
    X(FMUL, 0b0010, "fmul")                                                                                            \
    X(FDIV, 0b0011, "fdiv")                                                                                            \
    X(FMOV, 0b0100, "fmov")                                                                                            \
    X(FNEG, 0b0101, "fneg")                                                                                            \
    X(FABS, 0b0110, "fabs")                                                                                            \
    X(FSQRT, 0b0111, "fsqrt")                                                                                          \
    X(I32_TO_F, 0b1000, "i32tof")                                                                                      \
    X(F_TO_I32, 0b1001, "ftoi32")                                                                                      \
    X(I64_TO_F, 0b1010, "i64tof")                                                                                      \
    X(F_TO_I64, 0b1011, "ftoi64")                                                                                      \
    X(U32_TO_F, 0b1100, "u32tof")                                                                                      \
    X(F_TO_U32, 0b1101, "ftou32")                                                                                      \
    X(U64_TO_F, 0b1110, "u64tof")                                                                                      \
    X(F_TO_U64, 0b1111, "ftou64")

#define FloatOperationsEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        FloatOperationsValue(FloatOperationsEnum)
    };

#undef FloatOperationsEnum

    static constexpr Value values[] = {
        FADD,     FSUB,     FMUL,     FDIV,     FMOV,     FNEG,     FABS,     FSQRT,
        I32_TO_F, F_TO_I32, I64_TO_F, F_TO_I64, U32_TO_F, F_TO_U32, U64_TO_F, F_TO_U64,
    };

    constexpr FloatOperations(const Value raw) : _value(raw) {}

    constexpr FloatOperations(const Bits bits) : _value(Value(bits.Raw())) {}

    constexpr FloatOperations(const uint32_t bits) : _value(Value(bits)) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

    constexpr bool IsBasic() { return (_value >> 2u) == 0; }

    constexpr std::string_view ToStr()
    {
#define FloatOperationsStr(opc, value, str)                                                                            \
    case opc: return std::string_view(str);
        switch (_value) {
            FloatOperationsValue(FloatOperationsStr);
        }
        return std::string_view("<invalid>");
#undef FloatOperationsStr
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
        W8  = 0b00,
        W16 = 0b01,
        W32 = 0b10,
        W64 = 0b11,
    };

    constexpr Width(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

    constexpr uint32_t NBytes() const { return 1 << _value; }

    constexpr uint32_t NBits() const { return NBytes() * 8; }

    constexpr Bits Common() const
    {
        ASSERTION(_value == W32 || _value == W64, "TODO: format description");
        return _value & 1;
    }

    constexpr Bits FloatCast() const
    {
        ASSERTION(_value == W16 || _value == W64, "TODO: format description");
        return (_value & 0b10) >> 1;
    }

    constexpr std::string_view ToStr()
    {
        switch (_value) {
            case W8:  return "W8";
            case W16: return "W16";
            case W32: return "W32";
            case W64: return "W64";
        }
        return "<invalid>";
    }

    static constexpr Width FromCbcTypeKind(CbcTypeKind tkind)
    {
        switch (tkind) {
            case CbcTypeKind::I32: return Width::W32;
            case CbcTypeKind::U32: return Width::W32;
            case CbcTypeKind::F32: return Width::W32;
            case CbcTypeKind::F64: return Width::W64;
            case CbcTypeKind::I64: return Width::W64;
            case CbcTypeKind::U64: return Width::W64;

            default: ASSERT(false); return Width::W64;
        }
    }

private:
    Value _value;
};

class CC {
public:
#define CCValue(X)                                                                                                     \
    X(EQ, 0b0000, "eq")                                                                                                \
    X(NE, 0b0001, "ne")                                                                                                \
    X(LT, 0b0010, "lt")                                                                                                \
    X(GE, 0b0011, "ge")                                                                                                \
    X(ULT, 0b0100, "ult")                                                                                              \
    X(UGE, 0b0101, "uge")                                                                                              \
    X(REQ, 0b0110, "req")                                                                                              \
    X(RNE, 0b0111, "rne")                                                                                              \
    X(FEQ, 0b1000, "feq")                                                                                              \
    X(FNE, 0b1001, "fne")                                                                                              \
    X(FLT, 0b1010, "flt")                                                                                              \
    X(FNLT, 0b1011, "fnlt")                                                                                            \
    X(FGE, 0b1100, "fge")                                                                                              \
    X(FNGE, 0b1101, "fnge")                                                                                            \
    X(TESTZ, 0b1110, "z")                                                                                              \
    X(TESTNZ, 0b1111, "nz")

#define CCEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        CCValue(CCEnum)
    };

#undef CCEnum

    constexpr CC(const uint32_t raw) : _value((Value)raw) {}

    constexpr CC(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static CC From(uint8_t value) { return Value(value); }

    constexpr Bits ToBits() const { return _value; }

    constexpr bool IsRef() const { return _value == REQ || _value == RNE; }

    constexpr bool IsFloatingPoint() const { return _value >= FEQ && _value <= FNGE; }

    constexpr bool IsSigned() const { return _value < ULT || _value > RNE; }

    constexpr CC Negated(const uint32_t negated) const { return _value ^ negated; }

    constexpr std::string_view ToStr()
    {
#define CCStr(opc, value, str)                                                                                         \
    case opc: return std::string_view(str);
        switch (_value) {
            CCValue(CCStr);
        }
        return std::string_view("<invalid>");
#undef CCStr
    }

private:
    Value _value;
};

class OPC {
public:
    constexpr OPC(Sign sign, Bits b) : bits(Bits(sign).In(1).Shift(4) | b.In(4)) {}

    constexpr OPC(Common op, Bits b) : bits(op.ToBits().In(4).Shift(1) | b.In(1)) {}

    constexpr OPC(Common op, Width w) : OPC(op, w.Common()) {}

    constexpr OPC(Common op, Sign s) : OPC(op, Bits(s)) {}

    constexpr operator Bits() const { return bits; }

    constexpr Bits ToBits() const { return *this; }

private:
    Bits bits;
};

class StoreAccessKind {
public:
#define StoreAccessKindValue(X)                                                                                        \
    X(ST_8, 0b0000, "8")                                                                                               \
    X(ST_16, 0b0001, "16")                                                                                             \
    X(ST_32, 0b0010, "32")                                                                                             \
    X(ST_64, 0b0011, "64")                                                                                             \
    X(ST_REF, 0b0100, "ref")                                                                                           \
    X(SPECIAL, 0b0101, "special")                                                                                      \
    X(ST_F32, 0b0110, "f32")                                                                                           \
    X(ST_F64, 0b0111, "f64")

#define StoreAccessKindEnum(opc, value, str) opc = value,

    enum Value : uint8_t {
        StoreAccessKindValue(StoreAccessKindEnum)
    };

#undef StoreAccessKindEnum

    constexpr StoreAccessKind(const uint8_t raw) : _value((Value)raw) {}

    constexpr StoreAccessKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

    constexpr bool IsFloat() const { return _value == ST_F32 || _value == ST_F64; }

    constexpr std::string_view ToStr()
    {
#define StoreAccessKindStr(opc, value, str)                                                                            \
    case opc: return std::string_view(str);
        switch (_value) {
            StoreAccessKindValue(StoreAccessKindStr);
        }
        return std::string_view("<invalid>");
#undef StoreAccessKindStr
    }

private:
    Value _value;
};

class LoadAccessKind {
public:
#define LoadAccessKindValue(X)                                                                                         \
    X(LD_U8, 0b0000, "u8")                                                                                             \
    X(LD_U16, 0b0001, "u16")                                                                                           \
    X(LD_32, 0b0010, "32")                                                                                             \
    X(SPECIAL, 0b0011, "special") /* unused in interpreter */                                                          \
    X(LD_S8, 0b0100, "s8")                                                                                             \
    X(LD_S16, 0b0101, "s16")                                                                                           \
    X(LD_F32, 0b0110, "f32")                                                                                           \
    X(LD_F64, 0b0111, "f64")                                                                                           \
    X(LD_U8TO64, 0b1000, "u8to64")   /* unused in interpreter */                                                       \
    X(LD_U16TO64, 0b1001, "u16to64") /* unused in interpreter */                                                       \
    X(LD_U32TO64, 0b1010, "u32to64") /* unused in interpreter */                                                       \
    X(LD_64, 0b1011, "64")                                                                                             \
    X(LD_S8TO64, 0b1100, "s8to64")   /* unused in interpreter */                                                       \
    X(LD_S16TO64, 0b1101, "s16to64") /* unused in interpreter */                                                       \
    X(LD_S32TO64, 0b1110, "s32to64")                                                                                   \
    X(LD_REF, 0b1111, "ref")

#define LoadAccessKindEnum(opc, value, str) opc = value,

    enum Value : uint8_t {
        LoadAccessKindValue(LoadAccessKindEnum)
    };

#undef LoadAccessKindEnum

    constexpr LoadAccessKind(const uint8_t raw) : _value((Value)raw) {}

    constexpr LoadAccessKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

    constexpr bool IsFloat() const { return _value == LD_F32 || _value == LD_F64; }

    constexpr std::string_view ToStr()
    {
#define LoadAccessKindStr(opc, value, str)                                                                             \
    case opc: return std::string_view(str);
        switch (_value) {
            LoadAccessKindValue(LoadAccessKindStr);
        }
        return std::string_view("<invalid>");
#undef LoadAccessKindStr
    }

private:
    Value _value;
};

constexpr Bits mask_bits(uint32_t b)
{
    ASSERTION(1 <= b && b <= 32, "Shift overflow");
    return 0xffffffff >> b;
}

/// 4 bit; register
class Reg {
public:
    constexpr Reg(IReg r) : _value(r.Raw()) {}

    constexpr Reg(FReg r) : _value(r.Raw()) {}

    constexpr Reg(uint8_t value) : _value(value) { ASSERT((_value & 0xff) == _value); }

    inline operator uint8_t() const { return static_cast<uint8_t>(_value); }

    inline IReg IR() const { return IReg::From(*this); }

    inline FReg FR() const { return FReg::From(*this); }

private:
    uint32_t _value;
};

/// 8 bit; two registers
struct RR {
    Reg x;
    Reg y;

    inline static RR Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return RR {
            .x = b & 0xf,
            .y = b >> 4,
        };
    }
};

/// 4 bit; immediate or enumerations
class Imm4 {
public:
    constexpr inline Imm4(uint8_t _imm) : imm(_imm) { ASSERT((_imm & 0xf) == _imm); }

    inline Imm4() : imm(0) {}

    constexpr Imm4(Format::CC cc) : Imm4(static_cast<uint8_t>(cc)) {}

    constexpr Imm4(Format::Common common) : Imm4(static_cast<uint8_t>(common)) {}

    constexpr Imm4(Format::FloatOperations fpOps) : imm(static_cast<uint8_t>(fpOps)) {}

    inline operator uint8_t() const { return imm; }

    inline Format::CC CC() const { return Format::CC(imm); }

    inline Format::Common Common() const { return Format::Common::Value(imm); }

    inline Format::FloatOperations FloatOperations() const { return Format::FloatOperations(imm); }

    inline Format::StoreAccessKind STK() const { return Format::StoreAccessKind::Value(imm); }

    inline Format::LoadAccessKind LDK() const { return Format::LoadAccessKind(imm); }

    inline IReg IR() const { return IReg::From(imm); }

private:
    uint8_t imm;
};

/// 8 bit; immediate
struct Imm8 {
    uint8_t imm;

    inline static Imm8 Decode(Decoder::ByteReader& reader) { return Imm8 { reader.Read8() }; }

    inline operator uint8_t() const { return imm; }
};

/// 12 bit; immediate or literal
class Imm12 {
public:
    inline Imm12(uint16_t _imm) : imm(_imm) { ASSERT((_imm & 0xfff) == _imm); }

    inline Imm12() : imm(0) {}

    inline operator uint16_t() const { return imm; }

private:
    uint16_t imm;
};

/// 8 bit; Imm4 and register
struct XR {
    Imm4 imm;
    Reg r;

    inline static XR Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return XR {
            .imm = b & 0xf,
            .r   = b >> 4,
        };
    }
};

/// 16 bit; immediate or literal
struct Imm16 {
    uint16_t imm;

    inline static Imm16 Decode(Decoder::ByteReader& reader) { return Imm16 { reader.Read16() }; }

    inline operator uint16_t() const { return imm; }
};

/// 32 bit; immediate
union Imm32 {
    uint32_t imm;
    float fimm;

    inline static Imm32 Decode(Decoder::ByteReader& reader) { return Imm32 { reader.Read32() }; }
};

/// 48 bit; immediate
struct Imm48 {
    uint64_t imm;

    inline static Imm48 Decode(Decoder::ByteReader& reader)
    {
        uint32_t low32 = Imm32::Decode(reader).imm;
        uint64_t hi16  = Imm16::Decode(reader).imm;
        return Imm48 { (hi16 << 32) | low32 };
    }
};

/// 64 bit; immediate
union Imm64 {
    uint64_t imm;
    double dimm;

    inline static Imm64 Decode(Decoder::ByteReader& reader) { return Imm64 { reader.Read64() }; }
};

/// 16 bit; Imm4 and 12-bit immediate
struct XImm12 {
    Imm4 imm4;
    Imm12 imm12;

    inline static XImm12 Decode(Decoder::ByteReader& reader)
    {
        uint16_t b2 = reader.Read16();
        return XImm12 {
            .imm4  = b2 & 0xf,
            .imm12 = Imm12 { static_cast<uint16_t>(b2 >> 4) },
        };
    }

    inline static uint16_t Raw(XImm12 xi12) { return static_cast<uint16_t>(xi12.imm4 | (xi12.imm12 << 4)); }
};

/// 8 bit; Reg and 4-bit immediate
struct RImm4 {
    Reg r;
    Imm4 imm4;

    inline static RImm4 Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return RImm4 {
            .r    = b & 0xf,
            .imm4 = b >> 4,
        };
    }
};

/// 16 bit; Reg and 12-bit immediate
struct RImm12 {
    Reg r;
    Imm12 imm12;

    inline static RImm12 Decode(Decoder::ByteReader& reader)
    {
        uint16_t b2 = reader.Read16();
        return RImm12 {
            .r     = b2 & 0xf,
            .imm12 = Imm12 { static_cast<uint16_t>(b2 >> 4) },
        };
    }

    inline static uint16_t Raw(RImm12 xi12) { return static_cast<uint16_t>(xi12.r | (xi12.imm12 << 4)); }
};

} // namespace Format
} // namespace Cbc
