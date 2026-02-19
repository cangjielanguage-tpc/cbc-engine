#pragma once

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace API {

struct CbcTypeKind {
public:
    enum Value : uint8_t {
        INVALID, // 0x00
        VOID,    // 0x01
        U1,      // 0x02
        I8,      // 0x03
        U8,      // 0x04
        CHAR,    // 0x05
        I32,     // 0x06
        U32,     // 0x07
        F32,     // 0x08
        F64,     // 0x09
        I64,     // 0x0a
        U64,     // 0x0b
        NNREF,   // 0x0c aka non-nullable ref
        REF,     // 0x0d
        REC,     // 0x0e
        I16,     // 0x0f
        U16,     // 0x10
        F16,     // 0x11
        IN,      // 0x12
        UN,      // 0x13
        VA,      // 0x14
        TTI,     // 0x15
    };

    static_assert(INVALID == 0);

    constexpr CbcTypeKind(const Value value) : value(value) {}

    inline constexpr bool IsFloatingPoint() const { return value == F32 || value == F64; }

    inline constexpr bool IsNullableReference() const { return value == REF; }

    inline constexpr bool IsNonNullableReference() const { return value == NNREF; }

    inline constexpr bool IsReference() const { return IsNullableReference() || IsNonNullableReference(); }

    inline constexpr bool IsVArray() const { return value == VA; }

    inline constexpr bool IsRecord() const { return value == REC || IsVArray(); }

    constexpr bool IsPrimitive() const
    {
        switch (value) {
            case VOID:
            case U1:
            case I8:
            case U8:
            case I16:
            case U16:
            case F16:
            case I32:
            case U32:
            case F32:
            case CHAR:
            case I64:
            case U64:
            case IN:
            case UN:
            case F64:  return true;

            default: return false;
        }
    }

    bool IsSigned() const
    {
        switch (value) {
            case I8:
            case I16:
            case I32:
            case I64:
            case IN:  return true;
            case U8:
            case U16:
            case U32:
            case U64:
            case UN:  return false;

            default: {
                std::stringstream msg;
                msg << "Should not reach here: unexpected type kind " << value;
                throw std::runtime_error(msg.str());
            }
        }
    }

    int PrimSizeInBytes() const
    {
        switch (value) {
            case VOID: {
                return 0;
            }

            case U1:
            case I8:
            case U8: {
                return 1;
            }

            case I16:
            case U16:
            case F16: {
                return 2;
            }

            case I32:
            case U32:
            case F32:
            case CHAR: {
                return 4;
            }

            case I64:
            case U64:
            case IN:
            case UN:
            case F64: {
                return 8;
            }

            default: {
                std::stringstream msg;
                msg << "Should not reach here: unexpected type kind " << value;
                throw std::runtime_error(msg.str());
            }
        }
    }

    inline constexpr operator Value() const { return value; }

    std::string_view ToString() const
    {
        switch (value) {
            case INVALID: return "invalid";
            case VOID:    return "void";
            case U1:      return "i1";
            case I8:      return "i8";
            case U8:      return "u8";
            case CHAR:    return "char";
            case I32:     return "i32";
            case U32:     return "u32";
            case F32:     return "f32";
            case F64:     return "f64";
            case I64:     return "i64";
            case U64:     return "u64";
            case REF:     return "ref";
            case REC:     return "rec";
            case NNREF:   return "nnref";
            case I16:     return "i16";
            case U16:     return "u16";
            case F16:     return "f16";
            case IN:      return "inative";
            case UN:      return "unative";
            case VA:      return "varray";
            case TTI:     return "thistypeinfo";

            default: {
                std::stringstream msg;
                msg << "Should not reach here: unexpected type kind " << value;
                throw std::runtime_error(msg.str());
            }
        }
    }

private:
    static constexpr Value LAST = TTI;

    Value value;
};

} // namespace API
