#ifndef CBC_ISA_RT_H
#define CBC_ISA_RT_H

#include "isa.h"
#include "decoder.h"

namespace Cbc {
namespace RT {

constexpr int LIT_TABLE_SIZE = 4096;

class Opcode {
public:
    enum Value : uint8_t {
        HALT, // B1 TODO merge rare commands
        RET,  // B1 TODO merge rare commands
        MOV,  // B2rr
        MOVI, // B2xr
        MOVR, // B2rr
        FMOV, // B2rr
        MOVI2F, // B2rr
        MOVF2I, // B2rr
        FMOVI32, // B6xri32
        FMOVI64, // B10xri64

        BCC32I, // B4xi12rr
        BCC64I, // B4xi12rr
        BCC32L, // B4xi12rr
        BCC64L, // B4xi12rr
        BCCI32I, // B5xi12ri12
        BCCI64I, // B5xi12ri12
        BCCI32L, // B5xi12ri12
        BCCI64L, // B5xi12ri12
        BCCL32I, // B5xi12ri12
        BCCL64I, // B5xi12ri12
        BCCL32L, // B5xi12ri12
        BCCL64L, // B5xi12ri12
        JMP32, // B5i32

        BIN32, // B3xrrr
        BIN64, // B3xrrr
        BINI32I, // B4xi12rr
        BINI64I, // B4xi12rr
        BINI32L, // B4xi12rr
        BINI64L, // B4xi12rr
        FBIN32, // B3xrrr
        FBIN64, // B3xrrr

        NEWOBJ, // B3xi12,
        LOAD_OBJ, // B4xi12rr
        STORE_OBJ, // B4xi12rr

        MEMSPACE, // B1. See `MemOpcode`

        OPCODE_NUM,
    };
    static_assert(OPCODE_NUM <= 256);

    inline constexpr Opcode(const Value value) : _value(value) {}
    inline constexpr Opcode(const uint8_t raw) : _value(static_cast<Value>(raw)) {}
    inline constexpr Opcode() : _value(HALT) {}
    inline constexpr operator Value() const { return _value; }

    inline static Opcode Decode(Decoder::ByteReader& reader) {
        uint8_t b = reader.Read8();
        return Opcode(b);
    }

private:
    Value _value;
};

/// ISA12 is encoding a number of different mem.head operations
/// which are describing the kind of a base.
/// This is not convenient for interpretation,
/// since the actual value is only needed at the tail of memspace
/// in the operation itself (and sometimes it is not needed at all).
///
/// So, the ISA12 would require from memspace interpreter to store
/// the state of a head all the way to the tail.
///
/// Instead, we will encode memspace command as one `MEMSPACE` opcode,
/// which will enter the memspace, accumulate the offset and
/// use it to perform the actual operation.
///
/// Additionally, we would expect that the whole MEMSPACE instruction
/// will not throw any exception and must execute without errors from start to finish.
class MemOpcode {
public:
    enum Value : uint8_t {
        MEM_HALT, // M1

        OFFS16, // M2i16
        OFFS32, // M2i32
        OFFS64, // M2i64
        OFFS_REG, // M2xr

        RLD_START_OPCODE,
        RLD_U8 = RLD_START_OPCODE,  // M2rr
        RLD_U16, // M2rr
        RLD_32,  // M2rr
        RLD_S8,  // M2rr
        RLD_S16, // M2rr
        RLD_F32, // M2rr
        RLD_F64, // M2rr
        RLD_64,  // M2rr
        RLD_S32TO64, // M2rr
        RLD_REF, // M2rr
        RLD_END_OPCODE = RLD_REF,
        RST_START_OPCODE,
        RST_8 = RST_START_OPCODE,   // M2rr
        RST_16,  // M2rr
        RST_32,  // M2rr
        RST_64,  // M2rr
        RST_REF, // M2rr
        RST_F32, // M2rr
        RST_F64, // M2rr
        RST_END_OPCODE = RST_F64,

        // TODO: Add following opcodes.
        // SLD_*, <- struct
        // SST_*,
        // GLD_*, <- global
        // GST_*,
        // ULD_*, <- uts needed for marking (otherwise, use FLD)
        // UST_*, <- uts needed for marking (otherwise, use FST)
        // FLD_*, <- frame
        // FST_*,

        // TODO: Discuss which copy operations are needed,
        //       since there is up to N^2 possible combinations.
        // COPY,

        // TODO: Discuss how to implement index operation properly:
        //       handle OOB exception and different kinds of Array type.
        // INDEX

        OPCODE_NUM,
    };
    static_assert(OPCODE_NUM <= 256);

    inline constexpr MemOpcode(const Value value) : _value(value) {}
    inline constexpr MemOpcode(const uint32_t raw) : _value(static_cast<Value>(raw)) {}
    inline constexpr MemOpcode() : _value(MEM_HALT) {}
    inline constexpr operator Value() const { return _value; }

    inline static MemOpcode Decode(Decoder::ByteReader& reader) {
        uint8_t b = reader.Read8();
        return MemOpcode(b);
    }

private:
    Value _value;
};

/// 4 bit; register
class Reg {
public:
    constexpr Reg(IReg r) : _value(r) {}
    constexpr Reg(FReg r) : _value(r) {}
    constexpr Reg(uint8_t value) : _value(value) {
        assert((_value & 0xff) == _value);
    }

    inline operator uint8_t() const {
        return static_cast<uint8_t>(_value);
    }

    inline IReg IR() const {
        return IReg::From(*this);
    }

    inline FReg FR() const {
        return FReg::From(*this);
    }

private:
    uint32_t _value;
};

/// 8 bit; two registers
struct RR {
    Reg x;
    Reg y;

    inline static RR Decode(Decoder::ByteReader& reader) {
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
    constexpr inline Imm4(uint8_t _imm) : imm(_imm) {
        ASSERT((_imm & 0xf) == _imm);
    }

    inline Imm4() : imm(0) {}

    constexpr Imm4(Format::CC cc)
        : Imm4(static_cast<uint8_t>(cc)) {}

    constexpr Imm4(Format::Common common)
        : Imm4(static_cast<uint8_t>(common)) {}

    constexpr Imm4(Format::FloatOperations fpOps)
        : imm(static_cast<uint8_t>(fpOps)) {}

    inline operator uint8_t() const {
        return imm;
    }

    inline Format::CC CC() const {
        return Format::CC(imm);
    }

    inline Format::Common Common() const {
        return Format::Common(imm);
    }

    inline Format::FloatOperations FloatOperations() const {
        return Format::FloatOperations(imm);
    }

    inline Format::StoreAccessKind STK() const {
        return Format::StoreAccessKind(imm);
    }

    inline Format::LoadAccessKind LDK() const {
        return Format::LoadAccessKind(imm);
    }

    inline IReg IR() const {
        return IReg::From(imm);
    }

private:
    uint8_t imm;
};

/// 12 bit; immediate or literal
class Imm12 {
public:
    inline Imm12(uint16_t _imm) : imm(_imm) {
        assert((_imm & 0xfff) == _imm);
    }

    inline Imm12() : imm(0) {}

    inline operator uint16_t() const {
        return imm;
    }

private:
    uint16_t imm;
};

/// 8 bit; Imm4 and register
struct XR {
    Imm4 imm;
    Reg r;

    inline static XR Decode(Decoder::ByteReader& reader) {
        uint8_t b = reader.Read8();
        return XR {
            .imm = b & 0xf,
            .r = b >> 4,
        };
    }
};

/// 16 bit; immediate or literal
struct Imm16 {
    uint16_t imm;

    inline static Imm16 Decode(Decoder::ByteReader& reader) {
        return Imm16{reader.Read16()};
    }
};

/// 32 bit; immediate
union Imm32 {
    uint32_t imm;
    float fimm;

    inline static Imm32 Decode(Decoder::ByteReader& reader) {
        return Imm32{reader.Read32()};
    }
};

/// 64 bit; immediate
union Imm64 {
    uint64_t imm;
    double dimm;

    inline static Imm64 Decode(Decoder::ByteReader& reader) {
        return Imm64{reader.Read64()};
    }
};

/// 16 bit; Imm4 and 12-bit immediate
struct XImm12 {
    Imm4 imm4;
    Imm12 imm12;

    inline static XImm12 Decode(Decoder::ByteReader& reader) {
        uint16_t b2 = reader.Read16();
        return XImm12{
            .imm4 = b2 & 0xf,
            .imm12 = Imm12{static_cast<uint16_t>(b2 >> 4)},
        };
    }

    inline static uint16_t Raw(XImm12 xi12) {
        return static_cast<uint16_t>(xi12.imm4 | (xi12.imm12 << 4));
    }
};

/// 16 bit; Imm4 and 12-bit immediate
struct RImm12 {
    Reg r;
    Imm12 imm12;

    inline static RImm12 Decode(Decoder::ByteReader& reader) {
        uint16_t b2 = reader.Read16();
        return RImm12{
            .r = b2 & 0xf,
            .imm12 = Imm12{static_cast<uint16_t>(b2 >> 4)},
        };
    }

    inline static uint16_t Raw(RImm12 xi12) {
        return static_cast<uint16_t>(xi12.r | (xi12.imm12 << 4));
    }
};

struct B1 {
    Opcode opc;

    static B1 Decode(Decoder::ByteReader& reader) {
        return B1{Opcode::Decode(reader)};
    }
};

struct B2rr {
    Opcode opc;
    RR rr;

    inline static B2rr Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto rr = RR::Decode(reader);
        return B2rr{opc, rr};
    }
};

struct B2xr {
    Opcode opc;
    XR xr;

    inline static B2xr Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xr = XR::Decode(reader);
        return B2xr{opc, xr};
    }
};

struct B3xrrr {
    Opcode opc;
    XR xr;
    RR rr;

    static B3xrrr Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xr = XR::Decode(reader);
        auto rr = RR::Decode(reader);
        return B3xrrr{opc, xr, rr};
    }
};

struct B3xi12 {
    Opcode opc;
    XImm12 xi12;

    static B3xi12 Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xi12 = XImm12::Decode(reader);
        return B3xi12{opc, xi12};
    }
};

struct B4xi12rr {
    static constexpr int SIZE = 4;

    Opcode opc;
    XImm12 xi12;
    RR rr;

    static B4xi12rr Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xi12 = XImm12::Decode(reader);
        auto rr = RR::Decode(reader);
        return B4xi12rr{opc, xi12, rr};
    }
};

struct B4xi12xr {
    Opcode opc;
    XImm12 xi12;
    XR xr;

    static B4xi12xr Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xi12 = XImm12::Decode(reader);
        auto xr = XR::Decode(reader);
        return B4xi12xr{opc, xi12, xr};
    }
};

struct B5xi12ri12 {
    static constexpr int SIZE = 5;

    Opcode opc;
    XImm12 xi12;
    RImm12 ri12;

    static B5xi12ri12 Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xi12 = XImm12::Decode(reader);
        auto ri12 = RImm12::Decode(reader);
        return B5xi12ri12{opc, xi12, ri12};
    }
};

struct B5i32 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Imm32 imm32;

    static B5i32 Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto imm32 = Imm32::Decode(reader);
        return B5i32{opc, imm32};
    }
};

struct B6xri32 {
    static constexpr int SIZE = 6;

    Opcode opc;
    XR xr;
    Imm32 imm32;

    static B6xri32 Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xr = XR::Decode(reader);
        auto imm32 = Imm32::Decode(reader);
        return B6xri32{opc, xr, imm32};
    }
};

struct B10xri64 {
    static constexpr int SIZE = 9;

    Opcode opc;
    XR xr;
    Imm64 imm64;

    static B10xri64 Decode(Decoder::ByteReader& reader) {
        auto opc = Opcode::Decode(reader);
        auto xr = XR::Decode(reader);
        auto imm64 = Imm64::Decode(reader);
        return B10xri64{opc, xr, imm64};
    }
};

struct M2i16 {
    MemOpcode opc;
    uint16_t imm16;

    inline static M2i16 Decode(Decoder::ByteReader& reader) {
        auto opc = MemOpcode::Decode(reader);
        auto imm16 = reader.Read16();
        return M2i16{opc, imm16};
    }
};

struct M2i32 {
    MemOpcode opc;
    uint32_t imm32;

    inline static M2i32 Decode(Decoder::ByteReader& reader) {
        auto opc = MemOpcode::Decode(reader);
        auto imm32 = reader.Read32();
        return M2i32{opc, imm32};
    }
};

struct M2i64 {
    MemOpcode opc;
    uint64_t imm64;

    inline static M2i64 Decode(Decoder::ByteReader& reader) {
        auto opc = MemOpcode::Decode(reader);
        auto imm64 = reader.Read64();
        return M2i64{opc, imm64};
    }
};

struct M2rr {
    MemOpcode opc;
    RR rr;

    inline static M2rr Decode(Decoder::ByteReader& reader) {
        auto opc = MemOpcode::Decode(reader);
        auto rr = RR::Decode(reader);
        return M2rr{opc, rr};
    }
};

struct M2xr {
    MemOpcode opc;
    XR xr;

    inline static M2xr Decode(Decoder::ByteReader& reader) {
        auto opc = MemOpcode::Decode(reader);
        auto xr = XR::Decode(reader);
        return M2xr{opc, xr};
    }
};

} // namespace RT
} // namespace Cbc

#endif // CBC_ISA_RT_H
