#pragma once

#include <array>
#include <cstdint>
#include <optional>
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

    static constexpr int COUNT_ISA_ONLY = 14;

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
    X(LSL, 0b1100, "lsl")                                                                                              \
    X(POW, 0b1101, "pow")

#define CommonEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        CommonValue(CommonEnum) LAST = POW
    };

#undef CommonEnum

    static constexpr auto EXT  = SUB;
    static constexpr auto MOV  = SDIV;
    static constexpr auto MVST = SREM;
    static constexpr auto MREF = MVST;

    static constexpr Value values[] = {
        ADD, SUB, MUL, AND, OR, XOR, SDIV, SREM, UDIV, UREM, LSR, ASR, LSL, POW,
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
    X(CPOW, 0b0111, "cpow")                                                                                            \
    X(CLSL, 0b1000, "clsl")                                                                                            \
    X(CLSR, 0b1001, "clsr")                                                                                            \
    X(CASR, 0b1010, "casr")                                                                                            \
    X(CNEG, 0b1011, "cneg")

#define CheckedEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        CheckedValue(CheckedEnum) LAST = CNEG
    };

#undef CheckedEnum

    static constexpr Value values[] = { CADD, CSUB, CMUL, CDIV, CUADD, CUSUB, CUMUL, CPOW, CLSL, CLSR, CASR, CNEG };

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

class Saturating {
public:
#define SaturatingValue(X)                                                                                             \
    X(SADD, 0b0000, "sadd")                                                                                            \
    X(SSUB, 0b0001, "ssub")                                                                                            \
    X(SMUL, 0b0010, "smul")                                                                                            \
    X(SDIV, 0b0011, "sdiv")                                                                                            \
    X(SMOD, 0b0100, "smod")                                                                                            \
    X(SPOW, 0b0101, "spow")                                                                                            \
    X(SLSL, 0b0110, "slsl")                                                                                            \
    X(SASR, 0b0111, "sasr")                                                                                            \
    X(SUADD, 0b1000, "suadd")                                                                                          \
    X(SUSUB, 0b1001, "susub")                                                                                          \
    X(SUMUL, 0b1010, "sumul")                                                                                          \
    X(SUDIV, 0b1011, "sudiv")                                                                                          \
    X(SUMOD, 0b1100, "sumod")                                                                                          \
    X(SULSL, 0b1101, "sulsl")                                                                                          \
    X(SULSR, 0b1110, "sulsr")

#define SaturatingEnum(opc, value, str) opc = value,

    enum Value : uint32_t {
        SaturatingValue(SaturatingEnum) LAST = SULSR
    };

#undef SaturatingEnum

    static constexpr Value values[] = { SADD,  SSUB,  SMUL,  SDIV,  SMOD,  SPOW,  SLSL, SASR,
                                        SUADD, SUSUB, SUMUL, SUDIV, SUMOD, SULSL, SULSR };

    constexpr Saturating(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static Saturating From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr operator uint8_t() const { return static_cast<uint8_t>(_value); }

    constexpr const char* CStr() const
    {
#define SaturatingStr(opc, value, str)                                                                                 \
    case opc: return str;
        switch (_value) {
            SaturatingValue(SaturatingStr);
        }
        return "<invalid>";
#undef SaturatingStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

private:
    Value _value;
};

class FloatMathOp {
public:
#define FloatMathOpValue(X)                                                                                            \
    X(SIN, 0b000, "sin")                                                                                               \
    X(COS, 0b001, "cos")                                                                                               \
    X(LOG, 0b010, "log")                                                                                               \
    X(CEIL, 0b011, "ceil")                                                                                             \
    X(FLOOR, 0b100, "floor")

#define FloatMathOpEnum(opc, value, str) opc = value,

    enum Value : uint8_t {
        FloatMathOpValue(FloatMathOpEnum) LAST = FLOOR
    };

#undef FloatMathOpEnum

    static constexpr Value values[] = { SIN, COS, LOG, CEIL, FLOOR };

    constexpr FloatMathOp(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static FloatMathOp From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

    constexpr const char* CStr() const
    {
#define FloatMathOpStr(opc, value, str)                                                                                \
    case opc: return str;
        switch (_value) {
            FloatMathOpValue(FloatMathOpStr);
        }
        return "<invalid>";
#undef FloatMathOpStr
    }

    constexpr std::string_view ToStr() const { return std::string_view(CStr()); }

private:
    Value _value;
};

struct CastType {
    enum Code : uint8_t {
        I8  = 0x0,
        U8  = 0x1,
        I16 = 0x2,
        U16 = 0x3,
        I32 = 0x4,
        U32 = 0x5,
        I64 = 0x6,
        U64 = 0x7,
        F16 = 0x8,
        F32 = 0x9,
        F64 = 0xa,
        COUNT
    };

    struct Pair {
        Code from;
        Code to;
    };

    static constexpr uint8_t INVALID_CAST_CODE = 0xff;

    static constexpr std::string_view Name(Code code)
    {
        switch (code) {
            case I8:  return "I8";
            case U8:  return "U8";
            case I16: return "I16";
            case U16: return "U16";
            case I32: return "I32";
            case U32: return "U32";
            case I64: return "I64";
            case U64: return "U64";
            case F16: return "F16";
            case F32: return "F32";
            case F64: return "F64";
            default:  return "<invalid>";
        }
    }

    static constexpr bool IsFloatingPoint(Code code) { return code >= F16 && code <= F64; }
};

/// Maps `(from, to)` pairs to a dense index. Missing pairs are 0xff.
template <uint8_t N>
constexpr std::array<uint8_t, CastType::COUNT * CastType::COUNT> MakeCastPairTable(const CastType::Pair (&pairs)[N])
{
    std::array<uint8_t, CastType::COUNT * CastType::COUNT> table {};
    for (auto& entry : table) {
        entry = CastType::INVALID_CAST_CODE;
    }
    for (uint8_t i = 0; i < N; ++i) {
        table[pairs[i].from * CastType::COUNT + pairs[i].to] = i;
    }
    return table;
}

class CastOp {
public:
    using TypeCode = CastType::Code;
    using TypePair = CastType::Pair;

#define CAST_OP_LIST(X)                                                                                                \
    /* from I32 */                                                                                                     \
    X(I32_TO_I8, I32, I8)                                                                                              \
    X(I32_TO_U8, I32, U8)                                                                                              \
    X(I32_TO_I16, I32, I16)                                                                                            \
    X(I32_TO_U16, I32, U16)                                                                                            \
    X(I32_TO_I64, I32, I64)                                                                                            \
    X(I32_TO_F32, I32, F32)                                                                                            \
    X(I32_TO_F64, I32, F64)                                                                                            \
    /* from U32 */                                                                                                     \
    X(U32_TO_I8, U32, I8)                                                                                              \
    X(U32_TO_U8, U32, U8)                                                                                              \
    X(U32_TO_I16, U32, I16)                                                                                            \
    X(U32_TO_U16, U32, U16)                                                                                            \
    X(U32_TO_I64, U32, I64)                                                                                            \
    X(U32_TO_F32, U32, F32)                                                                                            \
    X(U32_TO_F64, U32, F64)                                                                                            \
    /* from I64 */                                                                                                     \
    X(I64_TO_I32, I64, I32)                                                                                            \
    X(I64_TO_F32, I64, F32)                                                                                            \
    X(I64_TO_F64, I64, F64)                                                                                            \
    /* from U64 */                                                                                                     \
    X(U64_TO_I32, U64, I32)                                                                                            \
    X(U64_TO_U32, U64, U32)                                                                                            \
    X(U64_TO_F32, U64, F32)                                                                                            \
    X(U64_TO_F64, U64, F64)                                                                                            \
    /* from F32 */                                                                                                     \
    X(F32_TO_I8, F32, I8)                                                                                              \
    X(F32_TO_U8, F32, U8)                                                                                              \
    X(F32_TO_I16, F32, I16)                                                                                            \
    X(F32_TO_U16, F32, U16)                                                                                            \
    X(F32_TO_I32, F32, I32)                                                                                            \
    X(F32_TO_U32, F32, U32)                                                                                            \
    X(F32_TO_I64, F32, I64)                                                                                            \
    X(F32_TO_U64, F32, U64)                                                                                            \
    X(F32_TO_F64, F32, F64)                                                                                            \
    /* from F64 */                                                                                                     \
    X(F64_TO_I8, F64, I8)                                                                                              \
    X(F64_TO_U8, F64, U8)                                                                                              \
    X(F64_TO_I16, F64, I16)                                                                                            \
    X(F64_TO_U16, F64, U16)                                                                                            \
    X(F64_TO_I32, F64, I32)                                                                                            \
    X(F64_TO_U32, F64, U32)                                                                                            \
    X(F64_TO_I64, F64, I64)                                                                                            \
    X(F64_TO_U64, F64, U64)                                                                                            \
    X(F64_TO_F32, F64, F32)

    enum Value : uint8_t {
#define CAST_OP_DECLARE(name, from, to) name,
        CAST_OP_LIST(CAST_OP_DECLARE)
#undef CAST_OP_DECLARE
        COUNT
    };

    static constexpr TypePair PAIRS[COUNT] = {
#define CAST_OP_PAIR(name, from, to) TypePair { TypeCode::from, TypeCode::to },
        CAST_OP_LIST(CAST_OP_PAIR)
#undef CAST_OP_PAIR
    };

#undef CAST_OP_LIST

    static constexpr auto PAIR_TABLE = MakeCastPairTable(PAIRS);

    constexpr static std::optional<CastOp> FromPair(uint8_t from, uint8_t to)
    {
        if (from >= CastType::COUNT || to >= CastType::COUNT) {
            return std::nullopt;
        }
        auto raw = PAIR_TABLE[from * CastType::COUNT + to];
        if (raw == CastType::INVALID_CAST_CODE) {
            return std::nullopt;
        }
        ASSERT(raw < COUNT);
        return CastOp(static_cast<Value>(raw));
    }

    constexpr CastOp(Value raw) : _value(raw) {}

    constexpr CastOp(uint8_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr operator Value() const { return _value; }

    constexpr uint8_t Raw() const { return static_cast<uint8_t>(_value); }

    constexpr bool IsValid() const { return Raw() < COUNT; }

    constexpr TypeCode FromType() const { return IsValid() ? PAIRS[_value].from : CastType::COUNT; }

    constexpr TypeCode ToType() const { return IsValid() ? PAIRS[_value].to : CastType::COUNT; }

    constexpr bool ToIsFloatingPoint() const { return CastType::IsFloatingPoint(ToType()); }

    constexpr bool FromIsFloatingPoint() const { return CastType::IsFloatingPoint(FromType()); }

    constexpr std::string_view ToTypeStr() const { return CastType::Name(ToType()); }

    constexpr std::string_view FromTypeStr() const { return CastType::Name(FromType()); }

private:
    Value _value;
};

class CheckedCastOp {
public:
    using TypeCode = CastType::Code;
    using TypePair = CastType::Pair;

#define CHECKED_CAST_OP_LIST(X)                                                                                        \
    /* from I8 */                                                                                                      \
    X(I8_TO_U8, I8, U8)                                                                                                \
    /* from U8 */                                                                                                      \
    X(U8_TO_I8, U8, I8)                                                                                                \
    /* from I16 */                                                                                                     \
    X(I16_TO_I8, I16, I8)                                                                                              \
    X(I16_TO_U8, I16, U8)                                                                                              \
    X(I16_TO_U16, I16, U16)                                                                                            \
    /* from U16 */                                                                                                     \
    X(U16_TO_I8, U16, I8)                                                                                              \
    X(U16_TO_U8, U16, U8)                                                                                              \
    X(U16_TO_I16, U16, I16)                                                                                            \
    /* from I32 */                                                                                                     \
    X(I32_TO_I8, I32, I8)                                                                                              \
    X(I32_TO_U8, I32, U8)                                                                                              \
    X(I32_TO_I16, I32, I16)                                                                                            \
    X(I32_TO_U16, I32, U16)                                                                                            \
    X(I32_TO_U32, I32, U32)                                                                                            \
    /* from U32 */                                                                                                     \
    X(U32_TO_I8, U32, I8)                                                                                              \
    X(U32_TO_U8, U32, U8)                                                                                              \
    X(U32_TO_I16, U32, I16)                                                                                            \
    X(U32_TO_U16, U32, U16)                                                                                            \
    X(U32_TO_I32, U32, I32)                                                                                            \
    /* from I64 */                                                                                                     \
    X(I64_TO_I8, I64, I8)                                                                                              \
    X(I64_TO_U8, I64, U8)                                                                                              \
    X(I64_TO_I16, I64, I16)                                                                                            \
    X(I64_TO_U16, I64, U16)                                                                                            \
    X(I64_TO_I32, I64, I32)                                                                                            \
    X(I64_TO_U32, I64, U32)                                                                                            \
    X(I64_TO_U64, I64, U64)                                                                                            \
    /* from U64 */                                                                                                     \
    X(U64_TO_I8, U64, I8)                                                                                              \
    X(U64_TO_U8, U64, U8)                                                                                              \
    X(U64_TO_I16, U64, I16)                                                                                            \
    X(U64_TO_U16, U64, U16)                                                                                            \
    X(U64_TO_I32, U64, I32)                                                                                            \
    X(U64_TO_U32, U64, U32)                                                                                            \
    X(U64_TO_I64, U64, I64)                                                                                            \
    /* from F32 */                                                                                                     \
    X(F32_TO_I8, F32, I8)                                                                                              \
    X(F32_TO_U8, F32, U8)                                                                                              \
    X(F32_TO_I16, F32, I16)                                                                                            \
    X(F32_TO_U16, F32, U16)                                                                                            \
    X(F32_TO_I32, F32, I32)                                                                                            \
    X(F32_TO_U32, F32, U32)                                                                                            \
    X(F32_TO_I64, F32, I64)                                                                                            \
    X(F32_TO_U64, F32, U64)                                                                                            \
    /* from F64 */                                                                                                     \
    X(F64_TO_I8, F64, I8)                                                                                              \
    X(F64_TO_U8, F64, U8)                                                                                              \
    X(F64_TO_I16, F64, I16)                                                                                            \
    X(F64_TO_U16, F64, U16)                                                                                            \
    X(F64_TO_I32, F64, I32)                                                                                            \
    X(F64_TO_U32, F64, U32)                                                                                            \
    X(F64_TO_I64, F64, I64)                                                                                            \
    X(F64_TO_U64, F64, U64)

    enum Value : uint8_t {
#define CHECKED_CAST_OP_DECLARE(name, from, to) name,
        CHECKED_CAST_OP_LIST(CHECKED_CAST_OP_DECLARE)
#undef CHECKED_CAST_OP_DECLARE
        COUNT
    };

    static constexpr TypePair PAIRS[COUNT] = {
#define CHECKED_CAST_OP_PAIR(name, from, to) TypePair { TypeCode::from, TypeCode::to },
        CHECKED_CAST_OP_LIST(CHECKED_CAST_OP_PAIR)
#undef CHECKED_CAST_OP_PAIR
    };

#undef CHECKED_CAST_OP_LIST

    static constexpr auto PAIR_TABLE = MakeCastPairTable(PAIRS);

    constexpr static std::optional<CheckedCastOp> FromPair(uint8_t from, uint8_t to)
    {
        if (from >= CastType::COUNT || to >= CastType::COUNT) {
            return std::nullopt;
        }
        auto raw = PAIR_TABLE[from * CastType::COUNT + to];
        if (raw == CastType::INVALID_CAST_CODE) {
            return std::nullopt;
        }
        ASSERT(raw < COUNT);
        return CheckedCastOp(static_cast<Value>(raw));
    }

    constexpr CheckedCastOp(Value raw) : _value(raw) {}

    constexpr CheckedCastOp(uint8_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr operator Value() const { return _value; }

    constexpr uint8_t Raw() const { return static_cast<uint8_t>(_value); }

    constexpr bool IsValid() const { return Raw() < COUNT; }

    constexpr TypeCode FromType() const { return IsValid() ? PAIRS[_value].from : CastType::COUNT; }

    constexpr TypeCode ToType() const { return IsValid() ? PAIRS[_value].to : CastType::COUNT; }

    constexpr bool ToIsFloatingPoint() const { return CastType::IsFloatingPoint(ToType()); }

    constexpr bool FromIsFloatingPoint() const { return CastType::IsFloatingPoint(FromType()); }

    constexpr std::string_view ToTypeStr() const { return CastType::Name(ToType()); }

    constexpr std::string_view FromTypeStr() const { return CastType::Name(FromType()); }

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
    X(F2I, 0b1001, "f2i")                                                                                              \
    X(FPOW, 0b1010, "fpow")

#define FloatOperationsEnum(opc, value, str) opc = value,

    enum Value : uint8_t {
        FloatOperationsValue(FloatOperationsEnum) LAST = FPOW
    };

#undef FloatOperationsEnum

    static constexpr Value values[] = { FADD, FSUB, FMUL, FDIV, FMOV, FNEG, FABS, FSQRT, I2F, F2I, FPOW };

    constexpr FloatOperations(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

    constexpr static FloatOperations From(uint8_t value)
    {
        ASSERT(value <= LAST);
        return Value(value);
    }

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
    constexpr Reg() : _value(0) {}

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

    constexpr Imm4(Format::Saturating saturating) : Imm4(static_cast<uint8_t>(saturating)) {}

    constexpr Imm4(Format::FloatOperations fpOps) : imm(static_cast<uint8_t>(fpOps)) {}

    constexpr Imm4(Format::FloatMathOp fpMathOp) : imm(static_cast<uint8_t>(fpMathOp)) {}

    inline operator uint8_t() const { return imm; }

    inline Format::CC CC() const { return Format::CC::From(imm); }

    inline Format::Common Common() const { return Format::Common::From(imm); }

    inline Format::Checked Checked() const { return Format::Checked::From(imm); }

    inline Format::Saturating Saturating() const { return Format::Saturating::From(imm); }

    inline Format::FloatOperations FloatOperations() const { return Format::FloatOperations::From(imm); }

    inline Format::FloatMathOp FloatMathOp() const { return Format::FloatMathOp::From(imm); }

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

    constexpr Imm8(Format::Saturating saturating) : Imm8(static_cast<uint8_t>(saturating)) {}

    inline Format::Checked Checked() const { return Format::Checked::From(imm); }

    inline Format::Saturating Saturating() const { return Format::Saturating::From(imm); }

    inline static Imm8 Decode(Decoder::ByteReader& reader) { return Imm8 { reader.Read8() }; }

    inline operator uint8_t() const { return imm; }
};

/// 12 bit; immediate or literal
class Imm12 {
public:
    inline Imm12(uint16_t _imm) : imm(_imm) { ASSERT((_imm & 0xF000) == 0 || (_imm & 0xF000) == 0xF000); }

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

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::FloatMathOp const op)
{
    return stream << op.ToStr();
}

inline Stream::Output& operator<<(Stream::Output& stream, Cbc::Format::Saturating const op)
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
