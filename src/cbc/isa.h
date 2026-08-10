#pragma once

#include <cstdint>
#include <string_view>

#include "cbc/decoder.h"
#include "isa_opcodes.h"
#include "utils/assertion.h"
#include "utils/ostream.h"

namespace Cbc {

class Opcode {
public:
#define DECLARE_OPCODE(opc, func) opc,
#define OPCODE_STR(opc, func)                                                                                          \
    case opc: return #opc;

    enum Value : uint8_t {
        ISA_OPCODES(DECLARE_OPCODE)
    };

    constexpr Opcode(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr const char* CStr()
    {
        switch (_value) {
            ISA_OPCODES(OPCODE_STR);
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

class MemOpcode {
public:
#define DECLARE_OPCODE(opc, func) opc,
#define OPCODE_STR(opc, func)                                                                                          \
    case opc: return #opc;

    enum Value : uint8_t {
        ISA_MEM_OPCODES(DECLARE_OPCODE)
    };

    constexpr MemOpcode(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr const char* CStr()
    {
        switch (_value) {
            ISA_MEM_OPCODES(OPCODE_STR);
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

class RegSymGroup {
public:
#define DECLARE_OPCODE(opc) opc,
#define OPCODE_STR(opc)                                                                                                \
    case opc: return #opc;

    enum Value : uint8_t {
        ISA_REG_SYM_GROUP_OPCODES(DECLARE_OPCODE)
    };

    constexpr RegSymGroup(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr static RegSymGroup From(uint8_t value)
    {
        ASSERT(value < Value::_END);
        return Value(value);
    }

    constexpr const char* CStr()
    {
        switch (_value) {
            ISA_REG_SYM_GROUP_OPCODES(OPCODE_STR);
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

class RegGroup {
public:
#define DECLARE_OPCODE(opc) opc,
#define OPCODE_STR(opc)                                                                                                \
    case opc: return #opc;

    enum Value : uint8_t {
        ISA_REG_GROUP_OPCODES(DECLARE_OPCODE)
    };

    constexpr RegGroup(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

    constexpr static RegGroup From(uint8_t value)
    {
        ASSERT(value < Value::_END);
        return Value(value);
    }

    constexpr const char* CStr()
    {
        switch (_value) {
            ISA_REG_GROUP_OPCODES(OPCODE_STR);
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

#undef OPCODE_STR
#undef DECLARE_OPCODE
private:
    Value _value;
};

#undef ISA_OPCODES

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
    X(IR13)                                                                                                            \
    X(IR_ACC)

#define IREG_ENUM(opc) opc,

    enum Value : uint8_t {
        IREG_VALUES(IREG_ENUM)
        // FIXME: This value is not first non-volatile register
        //        and is only used to shift non-volatile register mask!
        //        Rename it or fix mask-shifting logic in compiler.
        FIRST_NON_VOL = IR8
    };

    // Number of registers that compiler uses.
    static constexpr int VIRT_COUNT = 14;

    // Actual number of registers.
    static constexpr int COUNT = 15;

    constexpr IReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

#define IREG_TO_STR(opc)                                                                                               \
    case opc: return #opc;

    constexpr const char* CStr() const
    {
        switch (_value) {
            IREG_VALUES(IREG_TO_STR)
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

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
        FREG_VALUES(FREG_ENUM) FIRST_NON_VOL = FR8
    };

    static constexpr int COUNT = 16;

    constexpr FReg(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t Raw() const { return _value; }

#define FREG_TO_STR(opc)                                                                                               \
    case opc: return #opc;

    constexpr const char* CStr() const
    {
        switch (_value) {
            FREG_VALUES(FREG_TO_STR)
        }
        return "<invalid>";
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

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

class Sign {
public:
    enum Value : uint32_t {
        SIGNED   = 0b0,
        UNSIGNED = 0b1,
    };

    constexpr Sign(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

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

    constexpr operator Value() const { return _value; }

    constexpr static Common From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr const char* CStr() const
    {
#define CommonStr(opc, value, str)                                                                                     \
    case opc: return str;
        switch (_value) {
            CommonValue(CommonStr);
        }
        return "<invalid>";
#undef CommonStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

private:
    Value _value;
};

class Checked {
public:
#define CheckedValue(X)                                                                                                \
    X(CADD, 0b0000, "cadd")                                                                                            \
    X(CSUB, 0b0001, "csub")                                                                                            \
    X(CMUL, 0b0010, "cmul")                                                                                            \
    X(CDIV, 0b0011, "cdiv")                                                                                            \
    X(CUADD, 0b0100, "cuadd")                                                                                          \
    X(CUSUB, 0b0101, "cusub")                                                                                          \
    X(CUMUL, 0b0110, "cumul")                                                                                          \
    X(CPOW, 0b0111, "cpow")

#define CheckedEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        CheckedValue(CheckedEnum) LAST = CPOW
    };

#undef CheckedEnum

    static constexpr Value values[] = {
        CADD, CSUB, CMUL, CDIV, CUADD, CUSUB, CUMUL, CPOW,
    };

    constexpr Checked(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static Checked From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr operator uint8_t() const { return static_cast<uint8_t>(_value); }

    constexpr const char* CStr()
    {
#define CheckedStr(opc, value, str)                                                                                    \
    case opc: return str;
        switch (_value) {
            CheckedValue(CheckedStr);
        }
        return "<invalid>";
#undef CheckedStr
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

private:
    Value _value;
};

class ConvertType {
public:
#define ConvertTypeValue(X)                                                                                            \
    X(I8, 0x0, "I8", false)                                                                                            \
    X(U8, 0x1, "U8", false)                                                                                            \
    X(I16, 0x2, "I16", false)                                                                                          \
    X(U16, 0x3, "U16", false)                                                                                          \
    X(I32, 0x4, "I32", false)                                                                                          \
    X(U32, 0x5, "U32", false)                                                                                          \
    X(I64, 0x6, "I64", false)                                                                                          \
    X(U64, 0x7, "U64", false)                                                                                          \
    X(F16, 0x8, "F16", true)                                                                                           \
    X(F32, 0x9, "F32", true)                                                                                           \
    X(F64, 0xa, "F64", true)

#define ConvertTypeEnum(opc, value, str, fp) opc = value,

    enum Value : uint8_t {
        ConvertTypeValue(ConvertTypeEnum) LAST = F64
    };

#undef ConvertTypeEnum

    constexpr ConvertType(const uint8_t raw) : _value((Value)raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static ConvertType From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr std::string_view ToStr()
    {
#define ConvertTypeStr(opc, value, str, fp)                                                                            \
    case opc: return std::string_view(str);

        switch (_value) {
            ConvertTypeValue(ConvertTypeStr);
        }
        return std::string_view("<invalid>");

#undef ConvertTypeStr
    }

    constexpr bool IsFloatingPoint() const
    {
#define ConvertTypeFP(opc, value, str, fp)                                                                             \
    case opc: return fp;

        switch (_value) {
            ConvertTypeValue(ConvertTypeFP)
        }

#undef ConvertTypeFP
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
    X(I2F, 0b1000, "i2f")                                                                                              \
    X(F2I, 0b1001, "f2i")

#define FloatOperationsEnum(opc, value, str) opc = value,

    enum Value : uint8_t {
        FloatOperationsValue(FloatOperationsEnum) LAST = F2I
    };

#undef FloatOperationsEnum

    static constexpr Value values[] = { FADD, FSUB, FMUL, FDIV, FMOV, FNEG, FABS, FSQRT };

    constexpr FloatOperations(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static FloatOperations From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr bool IsBasic() { return (_value >> 2u) == 0; }

    constexpr const char* CStr() const
    {
#define FloatOperationsStr(opc, value, str)                                                                            \
    case opc: return str;
        switch (_value) {
            FloatOperationsValue(FloatOperationsStr);
        }
        return "<invalid>";
#undef FloatOperationsStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

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

        LAST = W64
    };

    constexpr Width(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr uint32_t NBytes() const { return 1 << _value; }

    constexpr uint32_t NBits() const { return NBytes() * 8; }

    constexpr static Width From(uint8_t value)
    {
        ASSERT(value <= Value::LAST);
        return Value(value);
    }

    constexpr const char* CStr() const
    {
        switch (_value) {
            case W8:  return "W8";
            case W16: return "W16";
            case W32: return "W32";
            case W64: return "W64";
        }
        return "<invalid>";
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

    constexpr bool IsRef() const { return _value == REQ || _value == RNE; }

    constexpr bool IsFloatingPoint() const { return _value >= FEQ && _value <= FNGE; }

    constexpr bool IsSigned() const { return _value < ULT || _value > RNE; }

    constexpr CC Negated(const uint32_t negated) const { return _value ^ negated; }

    constexpr const char* CStr()
    {
#define CCStr(opc, value, str)                                                                                         \
    case opc: return str;
        switch (_value) {
            CCValue(CCStr);
        }
        return "<invalid>";
#undef CCStr
    }

    constexpr std::string_view ToStr() { return std::string_view(CStr()); }

private:
    Value _value;
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
        StoreAccessKindValue(StoreAccessKindEnum) LAST = ST_F64
    };

#undef StoreAccessKindEnum

    constexpr StoreAccessKind(const Value raw) : _value(raw) {}

    constexpr static StoreAccessKind From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr operator Value() const { return _value; }

    constexpr bool IsFloat() const { return _value == ST_F32 || _value == ST_F64; }

    constexpr const char* CStr() const
    {
#define StoreAccessKindStr(opc, value, str)                                                                            \
    case opc: return str;
        switch (_value) {
            StoreAccessKindValue(StoreAccessKindStr);
        }
        return "<invalid>";
#undef StoreAccessKindStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

private:
    Value _value;
};

class LoadAccessKind {
public:
#define LoadAccessKindValue(X)                                                                                         \
    X(LD_U8, 0b0000, "u8")                                                                                             \
    X(LD_U16, 0b0001, "u16")                                                                                           \
    X(LD_32, 0b0010, "32")                                                                                             \
    X(LD_LEA, 0b0011, "lea")                                                                                           \
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
        LoadAccessKindValue(LoadAccessKindEnum) LAST = LD_REF
    };

#undef LoadAccessKindEnum

    constexpr LoadAccessKind(const Value raw) : _value(raw) {}

    constexpr static LoadAccessKind From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr operator Value() const { return _value; }

    constexpr bool IsFloat() const { return _value == LD_F32 || _value == LD_F64; }

    constexpr const char* CStr() const
    {
#define LoadAccessKindStr(opc, value, str)                                                                             \
    case opc: return str;
        switch (_value) {
            LoadAccessKindValue(LoadAccessKindStr);
        }
        return "<invalid>";
#undef LoadAccessKindStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

private:
    Value _value;
};

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

    constexpr Imm4(Format::Checked checked) : Imm4(static_cast<uint8_t>(checked)) {}

    constexpr Imm4(Format::FloatOperations fpOps) : imm(static_cast<uint8_t>(fpOps)) {}

    inline operator uint8_t() const { return imm; }

    inline Format::CC CC() const { return Format::CC::From(imm); }

    inline Format::Common Common() const { return Format::Common::From(imm); }

    inline Format::Checked Checked() const { return Format::Checked::From(imm); }

    inline Format::ConvertType ConvertType() const { return Format::ConvertType::From(imm); }

    inline Format::FloatOperations FloatOperations() const { return Format::FloatOperations::From(imm); }

    inline Format::StoreAccessKind STK() const { return Format::StoreAccessKind::From(imm); }

    inline Format::LoadAccessKind LDK() const { return Format::LoadAccessKind::From(imm); }

private:
    uint8_t imm;
};

/// 8 bit; immediate
struct Imm8 {
    uint8_t imm;

    constexpr inline Imm8(uint8_t _imm) : imm(_imm) {}

    constexpr Imm8(Format::Checked checked) : Imm8(static_cast<uint8_t>(checked)) {}

    inline Format::Checked Checked() const { return Format::Checked::From(imm); }

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

/// 8 bit; two Imm4
struct XX {
    Imm4 imm1;
    Imm4 imm2;

    inline static XX Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return XX {
            .imm1 = b & 0xf,
            .imm2 = b >> 4,
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
    void* ptr;
    uint64_t imm;
    double dimm;

    inline static Imm64 Decode(Decoder::ByteReader& reader) { return Imm64 { .imm = reader.Read64() }; }
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

namespace Stream {
inline Stream::Output& operator<<(Stream::Output& stream, Cbc::IReg const ireg) { return stream << ireg.ToStr(); }

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::FReg const freg) { return stream << freg.ToStr(); }

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::Width const width)
{
    return stream << width.CStr();
}

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::Common const op) { return stream << op.ToStr(); }

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::FloatOperations const op)
{
    return stream << op.ToStr();
}

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::LoadAccessKind const ldk)
{
    return stream << ldk.ToStr();
}

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::StoreAccessKind const stk)
{
    return stream << stk.ToStr();
}
} // namespace Stream
