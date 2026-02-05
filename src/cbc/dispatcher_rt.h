#pragma once

#include <type_traits>

#include "utils/assertion.h"
#include "utils/math.h"
#include "decoder.h"
#include "isa_rt.h"

namespace Cbc {
namespace RT {

using ImmKind = Cbc::Format::ImmKind;
using Width = Cbc::Format::Width;

template <typename Handler>
void InterpretationLoop(Handler handler, Decoder::ByteReader reader) {

#define NEXT goto *MAIN_TABLE[reader.PeekOpcode()]
#define NEXT_COND(successful) goto *MAIN_TABLE[(successful) ? reader.PeekOpcode() : 0]

#define MEM_NEXT goto *MEMSPACE_TABLE[reader.PeekOpcode()]

    static void* MAIN_TABLE[] = {
        &&HALT, // B1 TODO merge rare commands
        &&RET,  // B1 TODO merge rare commands
        &&MOV,  // B2rr
        &&MOVI,  // B2xr
        &&MOVR, // B2rr
        &&BCC32I, // B4xi12rr
        &&BCC64I, // B4xi12rr
        &&BCC32L, // B4xi12rr
        &&BCC64L, // B4xi12rr
        &&BCCI, // B4xi12xr

        &&BIN32, // B3xrrr
        &&BIN64, // B3xrrr
        &&BINI32I, // B4xi12rr
        &&BINI64I, // B4xi12rr
        &&BINI32L, // B4xi12rr
        &&BINI64L, // B4xi12rr

        &&NEWOBJ, // B3xri16,
        &&LOAD_OBJ, // B4xi12rr
        &&STORE_OBJ, // B4xi12rr

        &&MEMSPACE, // B1. See `MemOpcode`
    };

    static void* MEMSPACE_TABLE[] = {
        &&MEM_HALT, // M1

        &&OFFS16, // M2i16
        &&OFFS32, // M2i32
        &&OFFS64, // M2i64
        &&OFFS_REG, // M2xr

        &&RLD_U8,  // M2rr
        &&RLD_U16, // M2rr
        &&RLD_32,  // M2rr
        &&RLD_S8,  // M2rr
        &&RLD_S16, // M2rr
        &&RLD_F32, // M2rr
        &&RLD_F64, // M2rr
        &&RLD_64,  // M2rr
        &&RLD_S32TO64, // M2rr
        &&RLD_REF, // M2rr

        &&RST_8,   // M2rr
        &&RST_16,  // M2rr
        &&RST_32,  // M2rr
        &&RST_64,  // M2rr
        &&RST_REF, // M2rr
        &&RST_F32, // M2rr
        &&RST_F64, // M2rr

    };

    uint64_t memspaceOffsetAcc = 0;

    // jump to the instruction handler.
    NEXT;

    // -- Main opcode table --
    HALT: {
        ASSERTION(false, "halt");
        return;
    }
    RET: {
        return;
    }
    MOV: {
        auto args = B2rr::Decode(reader);
        handler.Mov(args.rr.x.IR(), args.rr.y.IR());
        NEXT;
    }
    MOVI: {
        auto args = B2xr::Decode(reader);
        handler.MovI(args.xr.r.IR(), MathUtils::SignExtend(args.xr.imm, 4));
        NEXT;
    }
    MOVR: {
        auto args = B2rr::Decode(reader);
        handler.MovRef(args.rr.x.IR(), args.rr.y.IR());
        NEXT;
    }
    BCC32I: {
        auto args = B4xi12rr::Decode(reader);
        int64_t delta = handler.template Bcc<ImmKind::VALUE, Width::W32>(args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCC32L: {
        auto args = B4xi12rr::Decode(reader);
        int64_t delta = handler.template Bcc<ImmKind::LITERAL, Width::W32>(args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCC64I: {
        auto args = B4xi12rr::Decode(reader);
        int64_t delta = handler.template Bcc<ImmKind::VALUE, Width::W64>(args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCC64L: {
        auto args = B4xi12rr::Decode(reader);
        int64_t delta = handler.template Bcc<ImmKind::LITERAL, Width::W64>(args.xi12.imm4.CC(), args.rr.x.IR(), args.rr.y.IR(), args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCI: {
        ASSERTION(false, "not implemented");
        return;
    }
    BIN32: {
        auto args = B3xrrr::Decode(reader);
        bool successful = handler.template Binary<Width::W32>(
                args.xr.imm.Common(), args.xr.r.IR(),
                args.rr.x.IR(), args.rr.y.IR());
        NEXT_COND(successful);
    }
    BIN64: {
        auto args = B3xrrr::Decode(reader);
        bool successful = handler.template Binary<Width::W64>(
                args.xr.imm.Common(), args.xr.r.IR(),
                args.rr.x.IR(), args.rr.y.IR());
        NEXT_COND(successful);
    }
    BINI32I: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.template BinaryImm<ImmKind::VALUE, Width::W32>(
                args.xi12.imm4.Common(), args.rr.x.IR(),
                args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    BINI64I: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.template BinaryImm<ImmKind::VALUE, Width::W64>(
                args.xi12.imm4.Common(), args.rr.x.IR(),
                args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    BINI32L: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.template BinaryImm<ImmKind::LITERAL, Width::W32>(
                args.xi12.imm4.Common(), args.rr.x.IR(),
                args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    BINI64L: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.template BinaryImm<ImmKind::LITERAL, Width::W64>(
                args.xi12.imm4.Common(), args.rr.x.IR(),
                args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    NEWOBJ: {
        auto args = B3xi12::Decode(reader);
        bool successful = handler.NewObj(args.xi12.imm4.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    LOAD_OBJ: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.LoadObj(args.xi12.imm4.LDK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    STORE_OBJ: {
        auto args = B4xi12rr::Decode(reader);
        bool successful = handler.StoreObj(args.xi12.imm4.STK(), args.rr.x, args.rr.y.IR(), args.xi12.imm12);
        NEXT_COND(successful);
    }
    MEMSPACE: {
        B1::Decode(reader);
        memspaceOffsetAcc = 0;
        MEM_NEXT;
    }

    // -- MemSpace opcode table --

    MEM_HALT: {
        ASSERTION(false, "halt");
        return;
    }

    OFFS16: {
        auto args = M2i16::Decode(reader);
        memspaceOffsetAcc += handler.MemOffset(args.imm16);
        MEM_NEXT;
    }
    OFFS32: {
        auto args = M2i32::Decode(reader);
        memspaceOffsetAcc += handler.MemOffset(args.imm32);
        MEM_NEXT;
    }
    OFFS64: {
        auto args = M2i64::Decode(reader);
        memspaceOffsetAcc += handler.MemOffset(args.imm64);
        MEM_NEXT;
    }
    OFFS_REG: {
        auto args = M2xr::Decode(reader);
        memspaceOffsetAcc += handler.MemOffsetReg(args.xr.r.IR());
        MEM_NEXT;
    }
#define RLD(ldk) \
    RLD_##ldk: {                                        \
        auto args = M2rr::Decode(reader);               \
        bool successful = handler.LoadObj(              \
                Format::LoadAccessKind::LD_##ldk,       \
                args.rr.x, args.rr.y.IR(),              \
                memspaceOffsetAcc);                     \
        NEXT_COND(successful);                          \
    }
    RLD(U8) RLD(U16) RLD(32) RLD(S8) RLD(S16) RLD(F32) RLD(F64) RLD(64) RLD(S32TO64) RLD(REF)
#undef RLD

#define RST(stk) \
    RST_##stk: {                                        \
        auto args = M2rr::Decode(reader);               \
        bool successful = handler.StoreObj(             \
                Format::StoreAccessKind::ST_##stk,      \
                args.rr.x, args.rr.y.IR(),              \
                memspaceOffsetAcc);                     \
        NEXT_COND(successful);                          \
    }
    RST(8) RST(16) RST(32) RST(64) RST(REF) RST(F32) RST(F64)
#undef RST

#undef MEM_NEXT
#undef NEXT
#undef NEXT_COND
}
} // namespace RT
} // namespace Cbc

