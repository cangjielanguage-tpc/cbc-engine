#pragma once

#include "decoder.h"
#include "isa.h"

// X parameters: opcode, encoding format, string format
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
    X(BCC64I, B4xi12rr, "bcc.64 $0cc $2ir $3ir $1I12")                                                                 \
    X(BCC32L, B4xi12rr, "bcc.32 $0cc $2ir $3ir $1I12L")                                                                \
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
    X(LOAD_OBJ, B4xi12rr, "ld.$0ldk $2ir [$3ir $1U12]")                                                                \
    X(STORE_OBJ, B4xi12rr, "st.$0stk $2ir [$3ir $1U12]")                                                               \
    X(LOAD_OBJ_F, B4xi12rr, "ld.$0ldk $2fr [$3ir $1U12]")                                                              \
    X(STORE_OBJ_F, B4xi12rr, "st.$0stk $2fr [$3ir $1U12]")                                                             \
    X(LOAD_ADDR, B2xr, "ld.addr.$0ldk")                                                                                \
    X(STORE_ADDR, B2xr, "st.addr.$0ldk")                                                                               \
    X(LOAD_REC, B4xi12rr, "ld.rec.$0ldk $2ir [$3ir $1U12]")                                                            \
    X(STORE_REC, B4xi12rr, "st.rec.$0stk $2ir [$3ir $1U12]")                                                           \
    X(LOAD_FRAME, B4xi12rr, "ld.frame.$0ldk $2ir [$3ir $1U12]")                                                        \
    X(STORE_FRAME, B4xi12rr, "st.frame.$0stk $2ir [$3ir $1U12]")                                                       \
    X(LOAD_REC_F, B4xi12rr, "ld.rec.$0ldk $2fr [$3ir $1U12]")                                                          \
    X(STORE_REC_F, B4xi12rr, "st.rec.$0stk $2fr [$3ir $1U12]")                                                         \
    X(LOAD_FRAME_F, B4xi12rr, "ld.frame.$0ldk $2fr [$3ir $1U12]")                                                      \
    X(STORE_FRAME_F, B4xi12rr, "st.frame.$0stk $2fr [$3ir $1U12]")                                                     \
    X(SCC32, B3xrrr, "scc.32 $0cc $1ir $2ir $3ir")                                                                     \
    X(SCC64, B3xrrr, "scc.64 $0cc $1ir $2ir $3ir")                                                                     \
    X(FSCC32, B3xrrr, "fscc.32 $0cc $1ir $2fr $3fr")                                                                   \
    X(FSCC64, B3xrrr, "fscc.64 $0cc $1ir $2fr $3fr")                                                                   \
    X(SCCI32I, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI64I, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12")                                                               \
    X(SCCI32L, B4xi12rr, "scci.32 $0cc $2ir $3ir $1I12L")                                                              \
    X(SCCI64L, B4xi12rr, "scci.64 $0cc $2ir $3ir $1I12L")                                                              \
    X(CONVERT, B3xxrr, "convert $0ct $1ct $2ir $3ir") /* FIXME: ir/fr */                                               \
    X(DIRECT_CALL_2I, B3xi12, "direct.call.2i $1I12L")                                                                 \
    X(DIRECT_CALL_2C, B3xi12, "direct.call.2c $1I12L")                                                                 \
    X(VIRTUAL_CALL_2C, B5i16i16, "virtual.call.2c $0U16L $1U16L")                                                      \
    X(MEMSPACE, B1, "memspace {")                                                                                      \
    X(GC_POINT, B1, "gcpoint")

// X parameters: opcode, encoding format, string format, is tail
#define CBC_RT_MEMOPCODES(X)                                                                                           \
    X(MEM_HALT, M1, "halt", true)                                                                                      \
    X(OFFS16, M3i16, "offs.16 $0U16", false)                                                                           \
    X(OFFS32, M5i32, "offs.32 $0U32", false)                                                                           \
    X(OFFS64, M9i64, "offs.64 $0U64", false)                                                                           \
    X(OFFS_REG, M2xr, "offs.r $1ir", false)                                                                            \
    X(RLD_U8, M2rr, "rld.u8 $0ir $1ir }", true)                                                                        \
    X(RLD_U16, M2rr, "rld.u16 $0ir $1ir }", true)                                                                      \
    X(RLD_32, M2rr, "rld.u32 $0ir $1ir }", true)                                                                       \
    X(RLD_S8, M2rr, "rld.s8 $0ir $1ir }", true)                                                                        \
    X(RLD_S16, M2rr, "rld.s16 $0ir $1ir }", true)                                                                      \
    X(RLD_F32, M2rr, "rld.f32 $0fr $1ir }", true)                                                                      \
    X(RLD_F64, M2rr, "rld.f64 $0fr $1ir }", true)                                                                      \
    X(RLD_64, M2rr, "rld.64 $0ir $1ir }", true)                                                                        \
    X(RLD_S32TO64, M2rr, "rld.s32to64 $0ir $1ir }", true)                                                              \
    X(RLD_REF, M2rr, "rld.ref $0ir $1ir }", true)                                                                      \
    X(RST_8, M2rr, "rst.8 $0ir $1ir }", true)                                                                          \
    X(RST_16, M2rr, "rst.16 $0ir $1ir }", true)                                                                        \
    X(RST_32, M2rr, "rst.32 $0ir $1ir }", true)                                                                        \
    X(RST_64, M2rr, "rst.64 $0ir $1ir }", true)                                                                        \
    X(RST_REF, M2rr, "rst.ref $0ir $1ir }", true)                                                                      \
    X(RST_F32, M2rr, "rst.f32 $0fr $1ir }", true)                                                                      \
    X(RST_F64, M2rr, "rst.f64 $0fr $1ir }", true)                                                                      \
    X(SLD_U8, M2rr, "sld.u8 $0ir $1ir }", true)                                                                        \
    X(SLD_U16, M2rr, "sld.u16 $0ir $1ir }", true)                                                                      \
    X(SLD_32, M2rr, "sld.u32 $0ir $1ir }", true)                                                                       \
    X(SLD_S8, M2rr, "sld.s8 $0ir $1ir }", true)                                                                        \
    X(SLD_S16, M2rr, "sld.s16 $0ir $1ir }", true)                                                                      \
    X(SLD_F32, M2rr, "sld.f32 $0fr $1ir }", true)                                                                      \
    X(SLD_F64, M2rr, "sld.f64 $0fr $1ir }", true)                                                                      \
    X(SLD_64, M2rr, "sld.64 $0ir $1ir }", true)                                                                        \
    X(SLD_S32TO64, M2rr, "sld.s32to64 $0ir $1ir }", true)                                                              \
    X(SLD_REF, M2rr, "sld.ref $0ir $1ir }", true)                                                                      \
    X(SST_8, M2rr, "sst.8 $0ir $1ir }", true)                                                                          \
    X(SST_16, M2rr, "sst.16 $0ir $1ir }", true)                                                                        \
    X(SST_32, M2rr, "sst.32 $0ir $1ir }", true)                                                                        \
    X(SST_64, M2rr, "sst.64 $0ir $1ir }", true)                                                                        \
    X(SST_REF, M2rr, "sst.ref $0ir $1ir }", true)                                                                      \
    X(SST_F32, M2rr, "sst.f32 $0fr $1ir }", true)                                                                      \
    X(SST_F64, M2rr, "sst.f64 $0fr $1ir }", true)                                                                      \
    X(FLD_U8, M2rr, "fld.u8 $0ir $1ir }", true)                                                                        \
    X(FLD_U16, M2rr, "fld.u16 $0ir $1ir }", true)                                                                      \
    X(FLD_32, M2rr, "fld.u32 $0ir $1ir }", true)                                                                       \
    X(FLD_S8, M2rr, "fld.s8 $0ir $1ir }", true)                                                                        \
    X(FLD_S16, M2rr, "fld.s16 $0ir $1ir }", true)                                                                      \
    X(FLD_F32, M2rr, "fld.f32 $0fr $1ir }", true)                                                                      \
    X(FLD_F64, M2rr, "fld.f64 $0fr $1ir }", true)                                                                      \
    X(FLD_64, M2rr, "fld.64 $0ir $1ir }", true)                                                                        \
    X(FLD_S32TO64, M2rr, "fld.s32to64 $0ir $1ir }", true)                                                              \
    X(FLD_REF, M2rr, "fld.ref $0ir $1ir }", true)                                                                      \
    X(FST_8, M2rr, "fst.8 $0ir $1ir }", true)                                                                          \
    X(FST_16, M2rr, "fst.16 $0ir $1ir }", true)                                                                        \
    X(FST_32, M2rr, "fst.32 $0ir $1ir }", true)                                                                        \
    X(FST_64, M2rr, "fst.64 $0ir $1ir }", true)                                                                        \
    X(FST_REF, M2rr, "fst.ref $0ir $1ir }", true)                                                                      \
    X(FST_F32, M2rr, "fst.f32 $0fr $1ir }", true)                                                                      \
    X(FST_F64, M2rr, "fst.f64 $0fr $1ir }", true)                                                                      \
    X(FSTI_8_8, M2i8, "fsti.8.8 $0U8 }", true)                                                                         \
    X(FSTI_16_8, M2i8, "fsti.16.8 $0U8 }", true)                                                                       \
    X(FSTI_16_16, M3i16, "fsti.16.16 $0U16 }", true)                                                                   \
    X(FSTI_32_8, M2i8, "fsti.32.8 $0U8 }", true)                                                                       \
    X(FSTI_32_16, M3i16, "fsti.32.16 $0U16 }", true)                                                                   \
    X(FSTI_32_32, M5i32, "fsti.32.32 $0U32 }", true)                                                                   \
    X(FSTI_64_8, M2i8, "fsti.64.8 $0U8 }", true)                                                                       \
    X(FSTI_64_16, M3i16, "fsti.64.16 $0U16 }", true)                                                                   \
    X(FSTI_64_32, M5i32, "fsti.64.32 $0U32 }", true)                                                                   \
    X(FSTI_64_64, M9i64, "fsti.64.64 $0U64 }", true)

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

    constexpr Opcode(const Value value) : _value(value) {}

    constexpr Opcode(const uint8_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr Opcode() : _value(HALT) {}

    constexpr operator Value() const { return _value; }

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
#define DEFINE_OPCODE(opc, dfmt, sfmt, isTail) opc,

    enum Value : uint8_t {
        CBC_RT_MEMOPCODES(DEFINE_OPCODE) OPCODE_NUM
    };

#undef DEFINE_OPCODE

    static constexpr auto RLD_START_OPCODE  = RLD_U8;
    static constexpr auto RLD_END_OPCODE    = RLD_REF;
    static constexpr auto SLD_START_OPCODE  = SLD_U8;
    static constexpr auto SLD_END_OPCODE    = SLD_REF;
    static constexpr auto FLD_START_OPCODE  = FLD_U8;
    static constexpr auto FLD_END_OPCODE    = FLD_REF;
    static constexpr auto RST_START_OPCODE  = RST_8;
    static constexpr auto RST_END_OPCODE    = RST_F64;
    static constexpr auto SST_START_OPCODE  = SST_8;
    static constexpr auto SST_END_OPCODE    = SST_F64;
    static constexpr auto FST_START_OPCODE  = FST_8;
    static constexpr auto FST_END_OPCODE    = FST_F64;
    static constexpr auto FSTI_START_OPCODE = FSTI_8_8;
    static constexpr auto FSTI_END_OPCODE   = FSTI_64_64;

    static_assert(OPCODE_NUM <= 256);

    constexpr MemOpcode(const Value value) : _value(value) {}

    constexpr MemOpcode(const uint32_t raw) : _value(static_cast<Value>(raw)) {}

    constexpr MemOpcode() : _value(MEM_HALT) {}

    constexpr operator Value() const { return _value; }

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

struct B3xxrr {
    Opcode opc;
    Format::XX xx;
    Format::RR rr;

    static B3xxrr Decode(Decoder::ByteReader& reader)
    {
        auto opc = Opcode::Decode(reader);
        auto xx  = Format::XX::Decode(reader);
        auto rr  = Format::RR::Decode(reader);
        return B3xxrr { opc, xx, rr };
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

struct B5i16i16 {
    static constexpr int SIZE = 5;

    Opcode opc;
    Format::Imm16 imm1;
    Format::Imm16 imm2;

    static B5i16i16 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = Opcode::Decode(reader);
        auto imm1 = Format::Imm16::Decode(reader);
        auto imm2 = Format::Imm16::Decode(reader);
        return B5i16i16 { opc, imm1, imm2 };
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

struct M1 {
    MemOpcode opc;

    static M1 Decode(Decoder::ByteReader& reader) { return M1 { MemOpcode::Decode(reader) }; }
};

struct M2i8 {
    MemOpcode opc;
    uint8_t imm8;

    inline static M2i8 Decode(Decoder::ByteReader& reader)
    {
        auto opc  = MemOpcode::Decode(reader);
        auto imm8 = reader.Read8();
        return M2i8 { opc, imm8 };
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
