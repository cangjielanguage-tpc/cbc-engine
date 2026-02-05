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
        &&BCCI32I, // B5xi12ri12
        &&BCCI64I, // B5xi12ri12
        &&BCCI32L, // B5xi12ri12
        &&BCCI64L, // B5xi12ri12
        &&BCCL32I, // B5xi12ri12
        &&BCCL64I, // B5xi12ri12
        &&BCCL32L, // B5xi12ri12
        &&BCCL64L, // B5xi12ri12
        &&JMP32, // B5i32

        &&BIN32, // B3xrrr
        &&BIN64, // B3xrrr
        &&BINI32I, // B4xi12rr
        &&BINI64I, // B4xi12rr
        &&BINI32L, // B4xi12rr
        &&BINI64L, // B4xi12rr

        &&NEWOBJ, // B3xri16,
        &&LOAD_OBJ, // B4xi12rr
        &&STORE_OBJ, // B4xi12rr
    };

    NEXT;

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
    BCCI32I: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W32>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCI64I: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::VALUE, ImmKind::VALUE, Width::W64>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCI32L: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W32>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCI64L: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::VALUE, ImmKind::LITERAL, Width::W64>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCL32I: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W32>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCL64I: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::LITERAL, ImmKind::VALUE, Width::W64>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCL32L: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W32>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    BCCL64L: {
        auto args = B5xi12ri12::Decode(reader);
        int64_t delta = handler.template BccImm<ImmKind::LITERAL, ImmKind::LITERAL, Width::W64>(args.xi12.imm4.CC(), args.ri12.r.IR(), args.ri12.imm12, args.xi12.imm12);
        reader.Advance(delta);
        NEXT;
    }
    JMP32: {
        auto args = B5i32::Decode(reader);
        int64_t delta = handler.Jmp(args.imm32.imm);
        reader.Advance(delta);
        NEXT;
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
#undef NEXT
#undef NEXT_COND
}
} // namespace RT
} // namespace Cbc

