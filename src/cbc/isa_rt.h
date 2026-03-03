#pragma once

#include "decoder.h"
#include "isa.h"

#define CBC_RT_OPCODES(X)                                                                                              \
    X(HALT, B1, "halt")                                                                                                \
    X(RET, B1, "ret")                                                                                                  \
    X(MOV, B2rr, "mov $0ir $1ir")                                                                                      \
    X(MOVI, B2xr, "movi $1ir $0I4")                                                                                    \
    X(MOVR, B2rr, "movr $0ir $1ir")                                                                                    \
    X(FMOV, B2rr, "fmov $0fr $1fr")                                                                                    \
    X(MOVI2F, B2rr, "i2f $0fr $1ir")                                                                                   \
    X(MOVF2I, B2rr, "f2i $0ir $1fr")                                                                                   \
    X(FMOVI32, B6xri32, "fmovi.32 $0fr $2F32")                                                                         \
    X(FMOVI64, B10xri64, "fmovi.64 $0fr $2F64")                                                                        \
    X(BCC32I, B4xi12rr, "bcc.32 $0cc $2ir $3ir $1I12")                                                                 \
    X(BCC64I, B4xi12rr, "bcc.64 $0cc $2ir $3ir $1I12L")                                                                \
    X(BCC32L, B4xi12rr, "bcc.32 $0cc $2ir $3ir $1I12")                                                                 \
    X(BCC64L, B4xi12rr, "bcc.64 $0cc $2ir $3ir $1I12L")                                                                \
    X(BCCI32I, B5xi12ri12, "bcc.32 $0cc $2ir $3I12 $1I12")                                                             \
    X(BCCI64I, B5xi12ri12, "bcc.64 $0cc $2ir $3I12 $1I12")                                                             \
    X(BCCI32L, B5xi12ri12, "bcc.32 $0cc $2ir $3I12 $1I12L")                                                            \
    X(BCCI64L, B5xi12ri12, "bcc.64 $0cc $2ir $3I12 $1I12L")                                                            \
    X(BCCL32I, B5xi12ri12, "bcc.32 $0cc $2ir $3I12L $1I12")                                                            \
    X(BCCL64I, B5xi12ri12, "bcc.64 $0cc $2ir $3I12L $1I12")                                                            \
    X(BCCL32L, B5xi12ri12, "bcc.32 $0cc $2ir $3I12L $1I12L")                                                           \
    X(BCCL64L, B5xi12ri12, "bcc.64 $0cc $2ir $3I12L $1I12L")                                                           \
    X(JMP32, B5i32, "jmp $0I32")                                                                                       \
    X(BIN32, B3xrrr, "$0bin.32 $1ir $2ir $3ir")                                                                        \
    X(BIN64, B3xrrr, "$0bin.64 $1ir $2ir $3ir")                                                                        \
    X(BINI32I, B4xi12rr, "$0bin.32 $2ir $3ir $1I12")                                                                   \
    X(BINI64I, B4xi12rr, "$0bin.64 $2ir $3ir $1I12")                                                                   \
    X(BINI32L, B4xi12rr, "$0bin.32 $2ir $3ir $1I12L")                                                                  \
    X(BINI64L, B4xi12rr, "$0bin.64 $2ir $3ir $1I12L")                                                                  \
    X(FBIN32, B3xrrr, "$0fop.32 $1fr $2fr $3fr")                                                                       \
    X(FBIN64, B3xrrr, "$0fop.64 $1fr $2fr $3fr")                                                                       \
    X(FUN32, B3xrrr, "$0fop.32 $1fr $3fr")                                                                             \
    X(FUN64, B3xrrr, "$0fop.64 $1fr $3fr")                                                                             \
    X(NEWOBJ, B3xi12, "newobj $0ir $1U12L")                                                                            \
    X(LOAD_OBJ, B4xi12rr, "ld.$0ldk $2r:$0ldk [$3ir $1U12]")                                                           \
    X(STORE_OBJ, B4xi12rr, "st.$0stk $2r:$0stk [$3ir $1U12]")                                                          \
    X(LOAD_REC, B4xi12rr, "ld.rec.$0ldk $2r:$0ldk [$3ir $1U12]")                                                       \
    X(STORE_REC, B4xi12rr, "st.rec.$0stk $2r:$0stk [$3ir $1U12]")                                                      \
    X(LOAD_FRAME, B4xi12rr, "ld.frame.$0ldk $2r:$0ldk [$3ir $1U12]")                                                   \
    X(STORE_FRAME, B4xi12rr, "st.frame.$0stk $2r:$0stk [$3ir $1U12]")                                                  \
    X(SCC32, B3xrrr, "scc.32 $0cc $1ir $2ir $3ir")                                                                     \
    X(SCC64, B3xrrr, "scc.64 $0cc $1ir $2ir $3ir")                                                                     \
    X(FSCC32, B3xrrr, "fscc.32 $0cc $1ir $2fr $3fr")                                                                   \
    X(FSCC64, B3xrrr, "fscc.64 $0cc $1ir $2fr $3fr")                                                                   \
    X(SCCI32I, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI64I, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI32L, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12L")                                                              \
    X(SCCI64L, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12L")                                                              \
    X(MEMSPACE, B1, "memspace {")

namespace Cbc {
namespace RT {

constexpr int LIT_TABLE_SIZE = 4096;

class Opcode {
public:
#define DEFINE_OPCODE(opc, dfmt, sfmt) opc,

    enum Value : uint8_t {
        CBC_RT_OPCODES(DEFINE_OPCODE) OPCODE_NUM
    };

#undef DEFINE_OPCODE

    static_assert(OPCODE_NUM <= 256);

    inline constexpr Opcode(const Value value) : _value(value) {}

    inline constexpr Opcode(const uint8_t raw) : _value(static_cast<Value>(raw)) {}

    inline constexpr Opcode() : _value(HALT) {}

    inline constexpr operator Value() const { return _value; }

    inline static Opcode Decode(Decoder::ByteReader& reader)
    {
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

        OFFS16,   // M2i16
        OFFS32,   // M2i32
        OFFS64,   // M2i64
        OFFS_REG, // M2xr

        RLD_START_OPCODE,
        RLD_U8 = RLD_START_OPCODE, // M2rr
        RLD_U16,                   // M2rr
        RLD_32,                    // M2rr
        RLD_S8,                    // M2rr
        RLD_S16,                   // M2rr
        RLD_F32,                   // M2rr
        RLD_F64,                   // M2rr
        RLD_64,                    // M2rr
        RLD_S32TO64,               // M2rr
        RLD_REF,                   // M2rr
        RLD_END_OPCODE = RLD_REF,
        RST_START_OPCODE,
        RST_8 = RST_START_OPCODE, // M2rr
        RST_16,                   // M2rr
        RST_32,                   // M2rr
        RST_64,                   // M2rr
        RST_REF,                  // M2rr
        RST_F32,                  // M2rr
        RST_F64,                  // M2rr
        RST_END_OPCODE = RST_F64,

        SLD_START_OPCODE,
        SLD_U8 = SLD_START_OPCODE, // M2rr
        SLD_U16,                   // M2rr
        SLD_32,                    // M2rr
        SLD_S8,                    // M2rr
        SLD_S16,                   // M2rr
        SLD_F32,                   // M2rr
        SLD_F64,                   // M2rr
        SLD_64,                    // M2rr
        SLD_S32TO64,               // M2rr
        SLD_REF,                   // M2rr
        SLD_END_OPCODE = SLD_REF,
        SST_START_OPCODE,
        SST_8 = SST_START_OPCODE, // M2rr
        SST_16,                   // M2rr
        SST_32,                   // M2rr
        SST_64,                   // M2rr
        SST_REF,                  // M2rr
        SST_F32,                  // M2rr
        SST_F64,                  // M2rr
        SST_END_OPCODE = SST_F64,

        FLD_START_OPCODE,
        FLD_U8 = FLD_START_OPCODE, // M2rr
        FLD_U16,                   // M2rr
        FLD_32,                    // M2rr
        FLD_S8,                    // M2rr
        FLD_S16,                   // M2rr
        FLD_F32,                   // M2rr
        FLD_F64,                   // M2rr
        FLD_64,                    // M2rr
        FLD_S32TO64,               // M2rr
        FLD_REF,                   // M2rr
        FLD_END_OPCODE = FLD_REF,
        FST_START_OPCODE,
        FST_8 = FST_START_OPCODE, // M2rr
        FST_16,                   // M2rr
        FST_32,                   // M2rr
        FST_64,                   // M2rr
        FST_REF,                  // M2rr
        FST_F32,                  // M2rr
        FST_F64,                  // M2rr
        FST_END_OPCODE = FST_F64,

        // TODO: Add following opcodes.
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

    inline static MemOpcode Decode(Decoder::ByteReader& reader)
    {
        uint8_t b = reader.Read8();
        return MemOpcode(b);
    }

private:
    Value _value;
};

class ImmKind {
public:
    enum Value : uint32_t {
        VALUE   = 0b00,
        LITERAL = 0b01,
    };

    constexpr ImmKind(const Value raw) : _value(raw) {}

    constexpr operator Value() const { return _value; }

private:
    Value _value;
};

struct B1 {
    Opcode opc;

    static B1 Decode(Decoder::ByteReader& reader) { return B1 { Opcode::Decode(reader) }; }
};

struct B2rr {
    Opcode opc;
    Format::RR rr;

    inline static B2rr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B2rr { opc, rr };
    }
};

struct B2xr {
    Opcode opc;
    Format::XR xr;

    inline static B2xr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        return B2xr { opc, xr };
    }
};

struct B3xrrr {
    Opcode opc;
    Format::XR xr;
    Format::RR rr;

    static B3xrrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B3xrrr { opc, xr, rr };
    }
};

struct B3xi12 {
    Opcode opc;
    Format::XImm12 xi12;

    static B3xi12 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        return B3xi12 { opc, xi12 };
    }
};

struct B4xi12rr {
    static constexpr int SIZE = 4;

    Opcode opc;
    Format::XImm12 xi12;
    Format::RR rr;

    static B4xi12rr Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto rr   = Format::RR::Decode(reader);
        return B4xi12rr { opc, xi12, rr };
    }
};

struct B4xi12xr {
    Opcode opc;
    Format::XImm12 xi12;
    Format::XR xr;

    static B4xi12xr Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto xr   = Format::XR::Decode(reader);
        return B4xi12xr { opc, xi12, xr };
    }
};

struct B5xi12ri12 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::XImm12 xi12;
    Format::RImm12 ri12;

    static B5xi12ri12 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto xi12 = Format::XImm12::Decode(reader);
        auto ri12 = Format::RImm12::Decode(reader);
        return B5xi12ri12 { opc, xi12, ri12 };
    }
};

struct B5i32 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::Imm32 imm32;

    static B5i32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B5i32 { opc, imm32 };
    }
};

struct B6xri32 {
    static constexpr int SIZE = 6;

    Opcode opc;
    Format::XR xr;
    Format::Imm32 imm32;

    static B6xri32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm32 = Format::Imm32::Decode(reader);
        return B6xri32 { opc, xr, imm32 };
    }
};

struct B10xri64 {
    static constexpr int SIZE = 9;

    Opcode opc;
    Format::XR xr;
    Format::Imm64 imm64;

    static B10xri64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = Opcode::Decode(reader);
        auto xr    = Format::XR::Decode(reader);
        auto imm64 = Format::Imm64::Decode(reader);
        return B10xri64 { opc, xr, imm64 };
    }
};

struct M3i16 {
    MemOpcode opc;
    uint16_t imm16;

    inline static M3i16 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm16 = reader.Read16();
        return M3i16 { opc, imm16 };
    }
};

struct M5i32 {
    MemOpcode opc;
    uint32_t imm32;

    inline static M5i32 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm32 = reader.Read32();
        return M5i32 { opc, imm32 };
    }
};

struct M9i64 {
    MemOpcode opc;
    uint64_t imm64;

    inline static M9i64 Decode(Decoder::ByteReader& reader)
    {
        auto opc   = MemOpcode::Decode(reader);
        auto imm64 = reader.Read64();
        return M9i64 { opc, imm64 };
    }
};

struct M2rr {
    MemOpcode opc;
    Format::RR rr;

    inline static M2rr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return M2rr { opc, rr };
    }
};

struct M2xr {
    MemOpcode opc;
    Format::XR xr;

    inline static M2xr Decode(Decoder::ByteReader& reader)
    {
        auto opc = MemOpcode::Decode(reader);
        auto xr  = Format::XR::Decode(reader);
        return M2xr { opc, xr };
    }
};

} // namespace RT
} // namespace Cbc
