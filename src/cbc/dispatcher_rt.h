#pragma once

#include "decoder.h"
#include "isa_rt.h"
#include "utils/assertion.h"
#include "utils/math.h"

#include "interpreter/interpreter.h"

namespace Cbc {
namespace RT {

using Width = Cbc::Format::Width;
using namespace Interpretation;

struct Thunk {
    void* function;
    void* arg;
};

template <typename RTI>
Thunk InterpretationLoop(
    Ectype* ectype, Frame* frame, ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
)
{
#define NEXT goto* MAIN_TABLE[reader.PeekOpcode()]
#define NEXT_COND(successful) goto* MAIN_TABLE[(successful) ? reader.PeekOpcode() : 0]
#define MEM_NEXT goto* MEMSPACE_TABLE[reader.PeekOpcode()]
    Interpretation::Interpreter<RTI> interpreter(ectype, frame, handle, literals);
    Decoder::ByteReader reader = reader0;

    static void* MAIN_TABLE[] = {
        &&HALT,    // B1 TODO merge rare commands
        &&RET,     // B1 TODO merge rare commands
        &&MOV,     // B2rr
        &&MOVI,    // B2xr
        &&MOVR,    // B2rr
        &&FMOV,    // B2rr
        &&MOVI2F,  // B2rr
        &&MOVF2I,  // B2rr
        &&FMOVI32, // B6xri32
        &&FMOVI64, // B10xri64
        &&BCC32I,  // B4xi12rr
        &&BCC64I,  // B4xi12rr
        &&BCC32L,  // B4xi12rr
        &&BCC64L,  // B4xi12rr
        &&BCCI32I, // B5xi12ri12
        &&BCCI64I, // B5xi12ri12
        &&BCCI32L, // B5xi12ri12
        &&BCCI64L, // B5xi12ri12
        &&BCCL32I, // B5xi12ri12
        &&BCCL64I, // B5xi12ri12
        &&BCCL32L, // B5xi12ri12
        &&BCCL64L, // B5xi12ri12
        &&JMP32,   // B5i32

        &&BIN32,   // B3xrrr
        &&BIN64,   // B3xrrr
        &&BINI32I, // B4xi12rr
        &&BINI64I, // B4xi12rr
        &&BINI32L, // B4xi12rr
        &&BINI64L, // B4xi12rr
        &&FBIN32,  // B3xrrr
        &&FBIN64,  // B3xrrr
        &&FUN32,   // B3xrrr
        &&FUN64,   // B3xrrr

        &&NEWOBJ,    // B3xri16,
        &&LOAD_OBJ,  // B4xi12rr
        &&STORE_OBJ, // B4xi12rr

        &&LOAD_REC,  // B4xi12rr
        &&STORE_REC, // B4xi12rr

        &&LOAD_FRAME,  // B4xi12rr
        &&STORE_FRAME, // B4xi12rr

        &&SCC32,   // B3xrrr
        &&SCC64,   // B3xrrr
        &&FSCC32,  // B3xrrr
        &&FSCC64,  // B3xrrr
        &&SCCI32I, // B4xi12rr
        &&SCCI64I, // B4xi12rr
        &&SCCI32L, // B4xi12rr
        &&SCCI64L, // B4xi12rr

        &&DIRECT_CALL, // B3xi12

        &&MEMSPACE, // B1. See `MemOpcode`
    };

    static void* MEMSPACE_TABLE[] = {
        &&MEM_HALT, // M1

        &&OFFS16,   // M2i16
        &&OFFS32,   // M2i32
        &&OFFS64,   // M2i64
        &&OFFS_REG, // M2xr

        &&RLD_U8,      // M2rr
        &&RLD_U16,     // M2rr
        &&RLD_32,      // M2rr
        &&RLD_S8,      // M2rr
        &&RLD_S16,     // M2rr
        &&RLD_F32,     // M2rr
        &&RLD_F64,     // M2rr
        &&RLD_64,      // M2rr
        &&RLD_S32TO64, // M2rr
        &&RLD_REF,     // M2rr

        &&RST_8,   // M2rr
        &&RST_16,  // M2rr
        &&RST_32,  // M2rr
        &&RST_64,  // M2rr
        &&RST_REF, // M2rr
        &&RST_F32, // M2rr
        &&RST_F64, // M2rr

        &&SLD_U8,      // M2rr
        &&SLD_U16,     // M2rr
        &&SLD_32,      // M2rr
        &&SLD_S8,      // M2rr
        &&SLD_S16,     // M2rr
        &&SLD_F32,     // M2rr
        &&SLD_F64,     // M2rr
        &&SLD_64,      // M2rr
        &&SLD_S32TO64, // M2rr
        &&SLD_REF,     // M2rr

        &&SST_8,   // M2rr
        &&SST_16,  // M2rr
        &&SST_32,  // M2rr
        &&SST_64,  // M2rr
        &&SST_REF, // M2rr
        &&SST_F32, // M2rr
        &&SST_F64, // M2rr

        &&FLD_U8,      // M2rr
        &&FLD_U16,     // M2rr
        &&FLD_32,      // M2rr
        &&FLD_S8,      // M2rr
        &&FLD_S16,     // M2rr
        &&FLD_F32,     // M2rr
        &&FLD_F64,     // M2rr
        &&FLD_64,      // M2rr
        &&FLD_S32TO64, // M2rr
        &&FLD_REF,     // M2rr

        &&FST_8,   // M2rr
        &&FST_16,  // M2rr
        &&FST_32,  // M2rr
        &&FST_64,  // M2rr
        &&FST_REF, // M2rr
        &&FST_F32, // M2rr
        &&FST_F64, // M2rr

    };

    uint64_t memspaceOffsetAcc = 0;

    // jump to the instruction handler.
    NEXT;

// -- Main opcode table --
HALT: {
    ASSERTION(false, "halt");
    return {};
}
RET: {
    return {};
}
MOV: {
    auto args = B2rr::Decode(reader);
    interpreter.Mov(args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
MOVI: {
    auto args = B2xr::Decode(reader);
    interpreter.MovI(args.xr.r.IR(), MathUtils::SignExtend(static_cast<uint64_t>(args.xr.imm), 4));
    NEXT;
}
MOVR: {
    auto args = B2rr::Decode(reader);
    interpreter.MovRef(args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
FMOV: {
    auto args = B2rr::Decode(reader);
    interpreter.Mov(args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
MOVI2F: {
    auto args = B2rr::Decode(reader);
    interpreter.Mov(args.rr.x.FR(), args.rr.y.IR());
    NEXT;
}
MOVF2I: {
    auto args = B2rr::Decode(reader);
    interpreter.Mov(args.rr.x.IR(), args.rr.y.FR());
    NEXT;
}
FMOVI32: {
    auto args = B6xri32::Decode(reader);
    interpreter.MovI(args.xr.r.FR(), args.imm32.fimm);
    NEXT;
}
FMOVI64: {
    auto args = B10xri64::Decode(reader);
    interpreter.MovI(args.xr.r.FR(), args.imm64.dimm);
    NEXT;
}
BCC32I: {
    auto args     = B4xi12rr::Decode(reader);
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCC32L: {
    auto args     = B4xi12rr::Decode(reader);
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCC64I: {
    auto args     = B4xi12rr::Decode(reader);
    int64_t delta = interpreter.template Bcc<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCC64L: {
    auto args     = B4xi12rr::Decode(reader);
    int64_t delta = interpreter.template Bcc<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCI32I: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCI64I: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCI32L: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCI64L: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCL32I: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCL64I: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCL32L: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
BCCL64L: {
    auto args     = B5xi12ri12::Decode(reader);
    int64_t delta = interpreter.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12
    );
    reader.Advance(delta);
    NEXT;
}
JMP32: {
    auto args     = B5i32::Decode(reader);
    int64_t delta = interpreter.Jmp(args.imm32.imm);
    reader.Advance(delta);
    NEXT;
}
BIN32: {
    auto args = B3xrrr::Decode(reader);
    bool successful =
        interpreter.template Binary<Width::W32>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
BIN64: {
    auto args = B3xrrr::Decode(reader);
    bool successful =
        interpreter.template Binary<Width::W64>(args.xr.imm.Common(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT_COND(successful);
}
BINI32I: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI64I: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.template BinaryImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI32L: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
BINI64L: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.template BinaryImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.Common(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT_COND(successful);
}
FBIN32: {
    auto args       = B3xrrr::Decode(reader);
    bool successful = interpreter.template Binary<Width::W32>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
FBIN64: {
    auto args       = B3xrrr::Decode(reader);
    bool successful = interpreter.template Binary<Width::W64>(
        args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.x.FR(), args.rr.y.FR()
    );
    NEXT_COND(successful);
}
FUN32: {
    auto args = B3xrrr::Decode(reader);
    bool successful =
        interpreter.template Unary<Width::W32>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
FUN64: {
    auto args = B3xrrr::Decode(reader);
    bool successful =
        interpreter.template Unary<Width::W64>(args.xr.imm.FloatOperations(), args.xr.r.FR(), args.rr.y.FR());
    NEXT_COND(successful);
}
NEWOBJ: {
    auto args       = B3xi12::Decode(reader);
    bool successful = interpreter.NewObj(args.xi12.imm4.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LOAD_OBJ: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.LoadObj(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_OBJ: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.StoreObj(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LOAD_REC: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.LoadRec(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_REC: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.StoreRec(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
    NEXT_COND(successful);
}
LOAD_FRAME: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.LoadFrame(args.xi12.imm4.LDK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
STORE_FRAME: {
    auto args       = B4xi12rr::Decode(reader);
    bool successful = interpreter.StoreFrame(args.xi12.imm4.STK(), args.rr.x, args.xi12.imm12);
    NEXT_COND(successful);
}
SCC32: {
    auto args = B3xrrr::Decode(reader);
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
SCC64: {
    auto args = B3xrrr::Decode(reader);
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.IR(), args.rr.y.IR());
    NEXT;
}
FSCC32: {
    auto args = B3xrrr::Decode(reader);
    interpreter.template SCC<Width::W32>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
FSCC64: {
    auto args = B3xrrr::Decode(reader);
    interpreter.template SCC<Width::W64>(args.xr.imm.CC(), args.xr.r.IR(), args.rr.x.FR(), args.rr.y.FR());
    NEXT;
}
SCCI32I: {
    auto args = B4xi12rr::Decode(reader);
    interpreter.template SCCImm<ImmKind::VALUE, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI64I: {
    auto args = B4xi12rr::Decode(reader);
    interpreter.template SCCImm<ImmKind::VALUE, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI32L: {
    auto args = B4xi12rr::Decode(reader);
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W32>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
SCCI64L: {
    auto args = B4xi12rr::Decode(reader);
    interpreter.template SCCImm<ImmKind::LITERAL, Width::W64>(
        args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12
    );
    NEXT;
}
DIRECT_CALL: {
    auto args    = B3xi12::Decode(reader);
    IReg dst     = args.xi12.imm4.IR();
    uint16_t imm = args.xi12.imm12;
    auto fuh     = reinterpret_cast<FunctionHandle*>(literals->at(imm).uintptr);
    // For proper support of fibers, the following call MUST drop the current frame.
    // This can not be guaranteed by C++ compiler consistently, because TCO
    // is not guaranteed and `mustcall` attribute is not supported
    // fully by gcc/clang compilers.
    //
    // Instead, the following call will drop the current frame manually
    // (outside of unit-test framework).

    reader0 = reader; // save current pc

    return { fuh->i2call, reinterpret_cast<void*>(fuh) };
}
MEMSPACE: {
    B1::Decode(reader);
    memspaceOffsetAcc = 0;
    MEM_NEXT;
}

    // -- MemSpace opcode table --

MEM_HALT: {
    ASSERTION(false, "halt");
    return {};
}

OFFS16: {
    auto args          = M3i16::Decode(reader);
    memspaceOffsetAcc += interpreter.MemOffset(args.imm16);
    MEM_NEXT;
}
OFFS32: {
    auto args          = M5i32::Decode(reader);
    memspaceOffsetAcc += interpreter.MemOffset(args.imm32);
    MEM_NEXT;
}
OFFS64: {
    auto args          = M9i64::Decode(reader);
    memspaceOffsetAcc += interpreter.MemOffset(args.imm64);
    MEM_NEXT;
}
OFFS_REG: {
    auto args          = M2xr::Decode(reader);
    memspaceOffsetAcc += interpreter.MemOffsetReg(args.xr.r.IR());
    MEM_NEXT;
}
#define RLD(ldk)                                                                                                       \
    RLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        bool successful =                                                                                              \
            interpreter.LoadObj(Format::LoadAccessKind::LD_##ldk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    RLD(U8)
    RLD(U16)
    RLD(32)
    RLD(S8)
    RLD(S16)
    RLD(F32)
    RLD(F64)
    RLD(64)
    RLD(S32TO64)
    RLD(REF)
#undef RLD

#define RST(stk)                                                                                                       \
    RST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        bool successful =                                                                                              \
            interpreter.StoreObj(Format::StoreAccessKind::ST_##stk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    RST(8)
    RST(16)
    RST(32)
    RST(64)
    RST(REF)
    RST(F32)
    RST(F64)
#undef RST

#define SLD(ldk)                                                                                                       \
    SLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        bool successful =                                                                                              \
            interpreter.LoadRec(Format::LoadAccessKind::LD_##ldk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    SLD(U8)
    SLD(U16)
    SLD(32)
    SLD(S8)
    SLD(S16)
    SLD(F32)
    SLD(F64)
    SLD(64)
    SLD(S32TO64)
    SLD(REF)
#undef SLD

#define SST(stk)                                                                                                       \
    SST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args = M2rr::Decode(reader);                                                                              \
        bool successful =                                                                                              \
            interpreter.StoreRec(Format::StoreAccessKind::ST_##stk, args.rr.x, args.rr.y.IR(), memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    SST(8)
    SST(16)
    SST(32)
    SST(64)
    SST(REF)
    SST(F32)
    SST(F64)
#undef SST

#define FLD(ldk)                                                                                                       \
    FLD_##ldk:                                                                                                         \
    {                                                                                                                  \
        auto args       = M2rr::Decode(reader);                                                                        \
        bool successful = interpreter.LoadFrame(Format::LoadAccessKind::LD_##ldk, args.rr.x, memspaceOffsetAcc);       \
        NEXT_COND(successful);                                                                                         \
    }
    FLD(U8)
    FLD(U16)
    FLD(32)
    FLD(S8)
    FLD(S16)
    FLD(F32)
    FLD(F64)
    FLD(64)
    FLD(S32TO64)
    FLD(REF)
#undef FLD

#define FST(stk)                                                                                                       \
    FST_##stk:                                                                                                         \
    {                                                                                                                  \
        auto args       = M2rr::Decode(reader);                                                                        \
        bool successful = interpreter.StoreFrame(Format::StoreAccessKind::ST_##stk, args.rr.x, memspaceOffsetAcc);     \
        NEXT_COND(successful);                                                                                         \
    }
    FST(8)
    FST(16)
    FST(32)
    FST(64)
    FST(REF)
    FST(F32)
    FST(F64)
#undef FST

#undef MEM_NEXT
#undef NEXT
#undef NEXT_COND
}
} // namespace RT
} // namespace Cbc
