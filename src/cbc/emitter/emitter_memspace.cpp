#include <cstring>
#include <utility>

#include "cbc/isa_rt.h"
#include "emitter.h"
#include "encoding_rt.h"
#include "utils/math.h"

namespace Cbc {
namespace Emitter {

using namespace Format;

using MemSpaceEmitter = Emitter::MemSpace;

MemSpaceEmitter Emitter::OpenMemSpace()
{
    RT::Opcode opc = RT::Opcode::MEMSPACE;
    Encode(segment, opc);
    return MemSpaceEmitter(*this);
}

void MemSpaceEmitter::Offset(uint64_t offset)
{
    if (MathUtils::IsNBits(offset, 16)) {
        Encode(
            segment,
            RT::M3i16 {
                .opc   = RT::MemOpcode::OFFS16,
                .imm16 = static_cast<uint16_t>(offset),
            }
        );
    } else if (MathUtils::IsNBits(offset, 32)) {
        Encode(
            segment,
            RT::M5i32 {
                .opc   = RT::MemOpcode::OFFS32,
                .imm32 = static_cast<uint32_t>(offset),
            }
        );
    } else {
        Encode(
            segment,
            RT::M9i64 {
                .opc   = RT::MemOpcode::OFFS64,
                .imm64 = static_cast<uint64_t>(offset),
            }
        );
    }
}

void MemSpaceEmitter::OffsetReg(IReg reg)
{
    Encode(
        segment,
        RT::M2xr {
            .opc = RT::MemOpcode::OFFS_REG,
            .xr  = XR { .imm = 0, .r = reg },
        }
    );
}

static RT::MemOpcode ComputeLoadAccessKind(LoadAccessKind ldk, RT::MemOpcode start)
{
    // This code is heavily rely on the fact that opcodes are ordered
    // in the same order as in the switch here.
    uint8_t delta = 0;
    switch (ldk) {
        case LoadAccessKind::LD_U8:      delta = 0; break;
        case LoadAccessKind::LD_U16:     delta = 1; break;
        case LoadAccessKind::LD_32:      delta = 2; break;
        case LoadAccessKind::LD_S8:      delta = 3; break;
        case LoadAccessKind::LD_S16:     delta = 4; break;
        case LoadAccessKind::LD_F32:     delta = 5; break;
        case LoadAccessKind::LD_F64:     delta = 6; break;
        case LoadAccessKind::LD_64:      delta = 7; break;
        case LoadAccessKind::LD_S32TO64: delta = 8; break;
        case LoadAccessKind::LD_REF:     delta = 9; break;
        default:                         FATAL("unexpected ldk: %d", ldk);
    }
    return RT::MemOpcode(start + delta);
}

static RT::MemOpcode ComputeStoreAccessKind(Format::StoreAccessKind stk, RT::MemOpcode start)
{
    // This code is heavily rely on the fact that opcodes are ordered
    // in the same order as in the switch here.
    uint8_t delta = 0;
    switch (stk) {
        case StoreAccessKind::ST_8:   delta = 0; break;
        case StoreAccessKind::ST_16:  delta = 1; break;
        case StoreAccessKind::ST_32:  delta = 2; break;
        case StoreAccessKind::ST_64:  delta = 3; break;
        case StoreAccessKind::ST_REF: delta = 4; break;
        case StoreAccessKind::ST_F32: delta = 5; break;
        case StoreAccessKind::ST_F64: delta = 6; break;
        default:                      FATAL("unexpected stk: %d", stk);
    }
    return RT::MemOpcode(start + delta);
}

void MemSpaceEmitter::LoadObj(LoadAccessKind ldk, Reg dst, IReg base)
{
    RT::MemOpcode opc = ComputeLoadAccessKind(ldk, RT::MemOpcode::RLD_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::RLD_END_OPCODE);
    LoadStore(ldk, dst, base, opc);
}

void MemSpaceEmitter::StoreObj(StoreAccessKind stk, Reg src, IReg base)
{
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::RST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::RST_END_OPCODE);
    LoadStore(stk, src, base, opc);
}

void MemSpaceEmitter::LoadRec(LoadAccessKind ldk, Reg dst, IReg base)
{
    RT::MemOpcode opc = ComputeLoadAccessKind(ldk, RT::MemOpcode::SLD_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::SLD_END_OPCODE);
    LoadStore(ldk, dst, base, opc);
}

void MemSpaceEmitter::StoreRec(StoreAccessKind stk, Reg src, IReg base)
{
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::SST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::SST_END_OPCODE);
    LoadStore(stk, src, base, opc);
}

void MemSpaceEmitter::LoadFrame(LoadAccessKind ldk, Reg dst)
{
    RT::MemOpcode opc = ComputeLoadAccessKind(ldk, RT::MemOpcode::FLD_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::FLD_END_OPCODE);
    LoadStore(ldk, dst, IReg::IRZ, opc);
}

void MemSpaceEmitter::StoreFrame(StoreAccessKind stk, Reg src)
{
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::FST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::FST_END_OPCODE);
    LoadStore(stk, src, IReg::IRZ, opc);
}

static RT::MemOpcode ComputeStoreImmStart(Format::StoreAccessKind stk, RT::MemOpcode start)
{
    uint8_t delta = 0;
    switch (stk) {
        case StoreAccessKind::ST_8:  break;
        case StoreAccessKind::ST_16: delta = 1; break;
        case StoreAccessKind::ST_32: delta = 3; break;
        case StoreAccessKind::ST_64: delta = 6; break;
        default:                     FATAL("unexpected stk: %d", stk);
    }
    return RT::MemOpcode(start + delta);
}

void MemSpaceEmitter::StoreFrameImm(StoreAccessKind stk, uint64_t imm)
{
    RT::MemOpcode opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::FSTI_START_OPCODE);

    if (MathUtils::IsNBitsSigned(imm, 8)) {
        ASSERT(opcStart <= RT::MemOpcode::FSTI_END_OPCODE);
        Encode(
            segment,
            RT::M2i8 {
                .opc  = opcStart,
                .imm8 = static_cast<uint8_t>(imm),
            }
        );
    } else if (MathUtils::IsNBitsSigned(imm, 16)) {
        RT::MemOpcode opc = RT::MemOpcode(opcStart + 1);
        ASSERT(stk != StoreAccessKind::ST_8 && opc <= RT::MemOpcode::FSTI_END_OPCODE);
        Encode(
            segment,
            RT::M3i16 {
                .opc   = opc,
                .imm16 = static_cast<uint16_t>(imm),
            }
        );
    } else if (MathUtils::IsNBitsSigned(imm, 32)) {
        RT::MemOpcode opc = RT::MemOpcode(opcStart + 2);
        ASSERT(
            (stk == StoreAccessKind::ST_32 || stk == StoreAccessKind::ST_64) && opc <= RT::MemOpcode::FSTI_END_OPCODE
        );
        Encode(
            segment,
            RT::M5i32 {
                .opc   = opc,
                .imm32 = static_cast<uint32_t>(imm),
            }
        );
    } else {
        RT::MemOpcode opc = RT::MemOpcode(opcStart + 3);
        ASSERT(stk == StoreAccessKind::ST_64 && opc <= RT::MemOpcode::FSTI_END_OPCODE);
        Encode(
            segment,
            RT::M9i64 {
                .opc   = opc,
                .imm64 = imm,
            }
        );
    }
}

} // namespace Emitter
} // namespace Cbc
