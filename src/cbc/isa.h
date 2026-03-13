#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "cbc/decoder.h"
#include "utils/assertion.h"
#include "utils/math.h"

namespace Cbc {

#include "isa_opcode_def.h"
typedef int opcode_t;
GEN_ENUM(DO_WITH_OPCODES(GET_OPC,GET_OPC,GET_OPC,GET_OPC,GET_OPC,GET_OPC,GET_OPC,GET_OPC))
#include "isa_opcode_undef.h"

//
// enum class opcode : opcode_t {
//     Mov32 = 0,
//     Mov64,
//     Mov32i,
//     Mov64i,
//     MovVst,
//     MovRef,
//     ExtendSigned,
//     ExtendUnsigned,
//     Add32,
//     Sub32,
//     Mul32,
//     And32,
//     Or32,
//     Xor32,
//     Div32Signed,
//     Rem32Signed,
//     Div32Unsigned,
//     Rem32Unsigned,
//     Lsr32,
//     Asr32,
//     Lsl32,
//     Neg32,
//     Add64,
//     Sub64,
//     Mul64,
//     And64,
//     Or64,
//     Xor64,
//     Div64Signed,
//     Rem64Signed,
//     Div64Unsigned,
//     Rem64Unsigned,
//     Lsr64,
//     Asr64,
//     Lsl64,
//     Neg64,
//     Add32Imm,
//     Sub32Imm,
//     Mul32Imm,
//     And32Imm,
//     Or32Imm,
//     Xor32Imm,
//     Div32SignedImm,
//     Rem32SignedImm,
//     Div32UnsignedImm,
//     Rem32UnsignedImm,
//     Lsr32Imm,
//     Asr32Imm,
//     Lsl32Imm,
//     Neg32Imm,
//     Add64Imm,
//     Sub64Imm,
//     Mul64Imm,
//     And64Imm,
//     Or64Imm,
//     Xor64Imm,
//     Div64SignedImm,
//     Rem64SignedImm,
//     Div64UnsignedImm,
//     Rem64UnsignedImm,
//     Lsr64Imm,
//     Asr64Imm,
//     Lsl64Imm,
//     Neg64Imm,
//     IntegerCommon32,
//     IntegerCommon64,
//     IntegerCommon32K0,
//     IntegerCommon64K0,
//     IntegerCommon32K8,
//     IntegerCommon64K8,
//     IntegerCommon32K16,
//     IntegerCommon64K16,
//     CheckedAdd,
//     CheckedSub,
//     CheckedMul,
//     CheckedDiv,
//     CheckedAddImm,
//     CheckedSubImm,
//     CheckedMulImm,
//     CheckedDivImm,
//     Bfx,
//     FloatCommon,
//     FloatMisc,
//     SetIf32,
//     SetIf64,
//     SetIf32Float,
//     SetIf64Float
// };
constexpr size_t opcode_sz = static_cast<size_t>(opcode::SetIf64Float);
constexpr opcode common32_base = opcode::ExtendUnsigned;
constexpr opcode common64_base = opcode::Neg32;
constexpr opcode common32imm_base = opcode::Neg64;
constexpr opcode common64imm_base = opcode::Neg32Imm;

enum class common_opc : opcode_t {
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
};
constexpr size_t common_opc_sz = static_cast<size_t>(common_opc::Lsl);

enum class float_opc : opcode_t {
    Add = 0,
    Sub,
    Mul,
    Div,
    Mov,
    Neg,
    Abs,
    Sqrt,
    Movi2f,
    Movf2i,
    FloatToFloat32,
    Float32ToFloat
};
constexpr size_t float_opc_sz = static_cast<size_t>(float_opc::Float32ToFloat);

enum class float_misc : opcode_t {
    ToInteger = 0,
    FromInteger
};

enum class checked_opc : opcode_t {
    Add = 0,
    Sub,
    Mul,
    Div
};
constexpr size_t checked_opc_sz = static_cast<size_t>(checked_opc::Div);

class IReg {
public:
    enum Value : uint8_t {
        IRZ,
        IR1,
        IR2,
        IR3,
        IR4,
        IR5,
        IR6,
        IR7,
        IR8,
        IR9,
        IR10,
        IR11,
        IR12,
        IR13,
    };

    static constexpr Value values[] = {
        IRZ, IR1, IR2, IR3, IR4, IR5, IR6, IR7, IR8, IR9, IR10, IR11, IR12, IR13,
    };

    static constexpr int COUNT = 14;

    constexpr IReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    inline static IReg From(const uint32_t raw)
    {
        ASSERT(raw < COUNT);
        return IReg(static_cast<Value>(raw));
    }

private:
    Value _value;
};

class FReg {
public:
    enum Value : uint32_t {
        FR0,
        FR1,
        FR2,
        FR3,
        FR4,
        FR5,
        FR6,
        FR7,
        FR8,
        FR9,
        FR10,
        FR11,
        FR12,
        FR13,
        FR14,
        FR15
    };

    static constexpr Value values[] = { FR0, FR1, FR2,  FR3,  FR4,  FR5,  FR6,  FR7,
                                        FR8, FR9, FR10, FR11, FR12, FR13, FR14, FR15 };

    static constexpr int COUNT = 16;

    constexpr FReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    inline static FReg From(const uint32_t raw)
    {
        ASSERT(raw < COUNT);
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
        CommonValue(CommonEnum)
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

    constexpr Bits ToBits() const { return _value; }

    constexpr bool B2rAllowed() { return (_value >> 3u) == 0; }

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

    static constexpr Value values[] = {
        W8,
        W16,
        W32,
        W64,
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

namespace B1 {
constexpr Bits FORMAT_BITS = 0b01001;
constexpr Bits MASK        = FORMAT_BITS.Shift(3);

constexpr Bits Fmt(Bits bits) { return MASK.Out(3) | bits.In(3); }
} // namespace B1

namespace B1piN {
enum Size : uint32_t {
    SZ_8  = 0b00,
    SZ_16 = 0b01,
    SZ_32 = 0b10,
    SZ_48 = 0b11,
};

constexpr Bits FORMAT_BITS = 0b01110;

constexpr uint32_t Fmt(Size sz, Sign sign)
{
    ASSERTION(!(sz == Size::SZ_48 && sign == Sign::UNSIGNED), "Not encodable");
    return (FORMAT_BITS.In(5).Shift(3) | Bits(sign).In(1).Shift(2) | Bits(sz).In(2)).Raw();
}
} // namespace B1piN

struct B2rr {
    Imm8 firstByte;
    RR rr;

    static inline B2rr Decode(Decoder::ByteReader& reader)
    {
        auto fb = Imm8::Decode(reader);
        auto rr = Format::RR::Decode(reader);
        return B2rr { fb, rr };
    }

    static constexpr Bits FORMAT_BITS = 0b0000;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(4);

    static constexpr Bits Fmt(Common op, Width width)
    {
        ASSERTION(op.B2rAllowed(), "not allowed in B2r format");
        return BYTE_MASK | OPC(op, width).ToBits().In(4);
    }

    static constexpr uint32_t Fmt(Common::Value op, Width::Value width) { return Fmt(Common(op), Width(width)).Raw(); }
};

struct B2hr {
    Imm8 firstByte;
    XR xr;

    static inline B2hr Decode(Decoder::ByteReader& reader)
    {
        auto fb = Imm8::Decode(reader);
        auto xr = Format::XR::Decode(reader);
        return B2hr { fb, xr };
    }

    static constexpr Bits FORMAT_BITS = 0b0001;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(4);

    static constexpr Bits Fmt(Common op, Bits b)
    {
        ASSERTION(op.B2rAllowed(), "not allowed in B2r format");
        return BYTE_MASK | OPC(op, b).ToBits().In(4);
    }

    static constexpr Bits Fmt(Common op, Width width) { return Fmt(op, width.Common()); }

    static constexpr uint32_t Fmt(Common::Value op, Width::Value width) { return Fmt(Common(op), Width(width)).Raw(); }

    static constexpr Bits Fmt(Common op, Sign sign) { return Fmt(op, sign.ToBits()); }

    static constexpr uint32_t Fmt(Common::Value op, Sign::Value sign) { return Fmt(Common(op), Sign(sign)).Raw(); }
};

namespace ConditionalBranch {
struct B2rrd8 {
    B2rr b2rr;
    Imm8 imm;

    static inline B2rrd8 Decode(Decoder::ByteReader& reader)
    {
        auto b2rr = B2rr::Decode(reader);
        auto imm  = Imm8::Decode(reader);
        return B2rrd8 { b2rr, imm };
    }

    static constexpr Bits FORMAT_BITS = 0b0101;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(4);

    static constexpr Bits Fmt(CC cc, Width w) { return BYTE_MASK | cc.ToBits().In(3).Shift(1) | w.Common(); }

    static constexpr uint32_t Fmt(CC::Value cc, Width::Value w) { return Fmt(CC(cc), Width(w)).Raw(); }
};

struct Continue {
    uint32_t offset;
    uint32_t negated;
};

struct C1dM {
    Imm8 fb;
    Imm32 d32;

    enum M : uint32_t {
        M8  = 0b00,
        M16 = 0b01,
        M32 = 0b11,
    };

    static constexpr Bits FORMAT_BITS = 0b10111;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(3);

    static constexpr uint32_t Fmt(M m, uint32_t negated)
    {
        return (BYTE_MASK | Bits(m).In(2).Shift(1) | Bits(negated).In(1)).Raw();
    }

    static inline C1dM Decode(Decoder::ByteReader& reader)
    {
        auto fb  = Imm8::Decode(reader);
        auto d32 = Imm32::Decode(reader);
        return C1dM { fb, d32 };
    }

    constexpr Continue ToContinue()
    {
        switch (fb.imm) {
            case Fmt(M::M32, 0): return Continue { d32.imm, 0 };
            case Fmt(M::M32, 1): return Continue { d32.imm, 1 };

            default: ASSERTION(false, "Unexpected format"); return Continue { 0, 0 };
        }
    }
};

struct B3xrrdT {
    Imm8 firstByte;
    XR xr;
    RImm4 rt4;

    static inline B3xrrdT Decode(Decoder::ByteReader& reader)
    {
        auto fb  = Imm8::Decode(reader);
        auto xr  = XR::Decode(reader);
        auto rt4 = RImm4::Decode(reader);
        return B3xrrdT { fb, xr, rt4 };
    }

    enum T : uint32_t {
        T8  = 0b00,
        T16 = 0b01,
        T0  = 0b10,
    };

    static constexpr Bits FORMAT_BITS = 0b1100;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(3);

    static constexpr uint32_t BranchIf(T t, uint32_t page)
    {
        return (BYTE_MASK | Bits(t).In(2).Shift(1) | Bits(page).In(1)).Raw();
    }

    static constexpr uint32_t BranchIfContinue(uint32_t page) { return BranchIf(T::T0, page); }
};

struct B2xri8d8 {
    Imm8 fb;
    XR xr;
    Imm8 imm;
    Imm8 dist;

    static constexpr Bits FORMAT_BITS = 0b0110011;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(1);

    static constexpr uint32_t Fmt(uint32_t page) { return (BYTE_MASK | Bits(page).In(1)).Raw(); }

    static inline B2xri8d8 Decode(Decoder::ByteReader& reader)
    {
        auto fb   = Imm8::Decode(reader);
        auto xr   = XR::Decode(reader);
        auto imm  = Imm8::Decode(reader);
        auto dist = Imm8::Decode(reader);
        return B2xri8d8 { fb, xr, imm, dist };
    }
};

struct B2xri16dM {
    Imm8 fb;
    XR xr;
    Imm16 imm;

    enum M : uint32_t {
        M0  = 0b0,
        M16 = 0b1,
    };

    static constexpr Bits FORMAT_BITS = 0b0110;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(4);

    static constexpr uint32_t BranchIf(M m, uint32_t page)
    {
        return (BYTE_MASK | Bits(0b10).Shift(2) | Bits(m).In(1).Shift(1) | Bits(page).In(1)).Raw();
    }

    static constexpr uint32_t BranchTypeTest(M m, uint32_t page)
    {
        return (BYTE_MASK | Bits(0b11).Shift(2) | Bits(m).In(1).Shift(1) | Bits(page).In(1)).Raw();
    }

    static inline B2xri16dM Decode(Decoder::ByteReader& reader)
    {
        auto fb  = Imm8::Decode(reader);
        auto xr  = XR::Decode(reader);
        auto imm = Imm16::Decode(reader);
        return B2xri16dM { fb, xr, imm };
    }
};
} // namespace ConditionalBranch

namespace SymbolicObjectControl {
constexpr Bits FORMAT_BITS = 0b1010;
constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(4);

constexpr Bits Fmt(uint32_t opc) { return BYTE_MASK | Bits(opc).In(4); }

struct B2xr {
    Imm8 fb;
    XR xr;

    static inline B2xr Decode(Decoder::ByteReader& reader)
    {
        auto fb = Imm8::Decode(reader);
        auto xr = XR::Decode(reader);
        return B2xr {
            .fb = fb,
            .xr = xr,
        };
    }
};

struct B2xrI {
    Imm8 fb;
    XR xr;
    Imm16 imm;

    static inline B2xrI Decode(Decoder::ByteReader& reader)
    {
        auto fb  = Imm8::Decode(reader);
        auto xr  = XR::Decode(reader);
        auto imm = Imm16::Decode(reader);
        return B2xrI { .fb = fb, .xr = xr, .imm = imm };
    }
};

class Opc0100 {
public:
    enum Value : uint32_t {
        CMP_RICH_IOF      = 0b0000,
        CMP_NOT_RICH_IOF  = 0b0001,
        CMP_RICH_EOP      = 0b0010,
        CMP_NOT_RICH_EOP  = 0b0011,
        CMP_CHA_TEST      = 0b0100,
        CMP_NOT_CHA_TEST  = 0b0101,
        THROW             = 0b0110,
        CATCH             = 0b0111,
        RET_32            = 0b1000,
        RET_64            = 0b1001,
        FRET_32           = 0b1010,
        FRET_64           = 0b1011,
        CHECK_DIV_ZERO_32 = 0b1100,
        CHECK_DIV_ZERO_64 = 0b1101,
        CHECK_NULL        = 0b1110,
    };

    constexpr static uint32_t OPCODE = SymbolicObjectControl::Fmt(0b0100).Raw();

    constexpr Opc0100(const uint8_t raw) : _value((Value)raw) {}

    constexpr Opc0100(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class Opc1000 {
public:
    enum Value : uint32_t {
        CALL_DIRECT       = 0b0000,
        CALL_VIRT         = 0b0001,
        CALL_INTERF_PLAIN = 0b0010,
        CALL_INTERF_RICH  = 0b0011,
        CALL_INTERF_EOP   = 0b0100,
        CALL_INDIRECT     = 0b0101,

        CALL_DIRECT_RESOLVED   = 0b0110,
        CALL_VIRT_RESOLVED     = 0b0111,
        CALL_INDIRECT_RESOLVED = 0b1000,
    };

    constexpr static uint32_t OPCODE = SymbolicObjectControl::Fmt(0b1000).Raw();

    constexpr Opc1000(const uint8_t raw) : _value((Value)raw) {}

    constexpr Opc1000(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};

class Opc1011 {
public:
    enum Value : uint32_t {
        NEWOBJ     = 0b0000,
        NEWOBJ_VST = 0b0001,
        NEWOBJ_R   = 0b0010, // TODO: not needed
    };

    constexpr static uint32_t OPCODE = SymbolicObjectControl::Fmt(0b1011).Raw();

    constexpr Opc1011(const uint8_t raw) : _value((Value)raw) {}

    constexpr Opc1011(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr Bits ToBits() const { return _value; }

private:
    Value _value;
};
} // namespace SymbolicObjectControl

struct B3xrrr {
    Imm8 fb;
    XR xr;
    RR rr;

    static inline B3xrrr Decode(Decoder::ByteReader& reader)
    {
        auto fb = Imm8::Decode(reader);
        auto xr = XR::Decode(reader);
        auto rr = RR::Decode(reader);
        return B3xrrr { .fb = fb, .xr = xr, .rr = rr };
    }

    static constexpr Bits FORMAT_BITS = 0b01000;
    static constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(3);

    static constexpr Bits Fmt(OP7A::Value op7a, Bits lowBit)
    {
        return BYTE_MASK | OP7A(op7a).ToBits().In(2).Shift(1) | lowBit.In(1);
    }

    static constexpr uint32_t Fmt(OP7A::Value op7a, Sign::Value sign) { return Fmt(op7a, Sign(sign).ToBits()).Raw(); }
};

namespace B3xrrt4i16 {
constexpr Bits FORMAT_BITS = 0b001;
constexpr Bits K_BITS      = 0b10; // for i16 imm
constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(5) | K_BITS.Shift(3);

constexpr Bits Fmt(Bits low3Bits) { return BYTE_MASK | low3Bits.In(3); }
} // namespace B3xrrt4i16

namespace B3xrrkI {
constexpr Bits FORMAT_BITS = 0b00111;
constexpr Bits BYTE_MASK   = FORMAT_BITS.Shift(3);

constexpr Bits Fmt(Bits low3Bits) { return BYTE_MASK | low3Bits.In(3); }
} // namespace B3xrrkI

namespace Immediate {
enum ImmKind : uint32_t {
    Signed,
    Unsigned,
    FloatingPoint,
};

class Decoding {
public:
    Decoding() : immext(0) {}

    void Reset() { immext = 0; }

    void SetImmExt(uint64_t v, uint32_t bits, Sign sign)
    {
        uint64_t extended = sign == Sign::SIGNED ? MathUtils::SignExtend(v, bits) : MathUtils::ZeroExtend(v, bits);
        immext            = extended << 16;
    }

    uint64_t StartDecoding(ImmKind kind, Width width, uint32_t N, uint64_t iN)
    {
        switch (kind) {
            case ImmKind::Signed:   return MathUtils::SignExtend(iN, N);
            case ImmKind::Unsigned: return MathUtils::ZeroExtend(iN, N);
            case ImmKind::FloatingPoint:
                if (width == Width::W32) {
                    return N < 16 ? Imm32 { .fimm = (float)MathUtils::SignExtend(static_cast<uint32_t>(iN), N) }.imm
                                  : MathUtils::ZeroExtend(iN, N);
                } else {
                    ASSERTION(width == Width::W64, "Unexpected width");
                    return N < 16 ? Imm64 { .dimm = (double)MathUtils::SignExtend(iN, N) }.imm
                                  : MathUtils::ZeroExtend(iN, N);
                }
            default: ASSERTION(false, "Unexpected imm kind"); return 0;
        }
    }

    uint64_t FinishDecoding(uint32_t W, uint32_t N, uint64_t ival, uint32_t rotCnt)
    {
        if (W == 32 || W == 64) {
            ival += immext & MathUtils::RightNBits64(W);
        }

        if (W >= 32 && W > N) {
            ival = W == 32 ? MathUtils::RotateRight32(static_cast<uint32_t>(ival), rotCnt)
                           : MathUtils::RotateRight64(ival, rotCnt);
        } else {
            ASSERTION(rotCnt == 0, "Invalid rotation count");
        }

        Reset();
        return ival;
    }

    uint64_t DecodeB2ri4(Width width, uint32_t i4)
    {
        uint64_t ival = StartDecoding(ImmKind::Signed, width, 4, i4);
        return FinishDecoding(width.NBits(), 4, ival, 0);
    }

    uint64_t DecodeIntegralBCCi16(Sign sign, Width width, uint32_t i16)
    {
        auto immKind  = sign == Sign::SIGNED ? ImmKind::Signed : ImmKind::Unsigned;
        uint64_t ival = StartDecoding(immKind, width, 16, i16);
        return FinishDecoding(width.NBits(), 16, ival, 0);
    }

private:
    uint64_t immext;
};

} // namespace Immediate

} // namespace Format
} // namespace Cbc
