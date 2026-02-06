#include <utility>
#include <cstring>

#include "emitter.h"
#include "cbc/isa_rt.h"
#include "utils/math.h"
#include "encoding_rt.h"

namespace Cbc {
namespace Emitter {

using Width = Format::Width;
using CC = Format::CC;
using Common = Format::Common;

using MemSpaceEmitter = Emitter::MemSpace;

MemSpaceEmitter Emitter::OpenMemSpace() {
    RT::Opcode opc = RT::Opcode::MEMSPACE;
    Encode(segment, opc);
    return MemSpaceEmitter(*this);
}

void MemSpaceEmitter::Offset(uint64_t offset) {
    if (MathUtils::IsNBits(offset, 16)) {
        Encode(segment, RT::M2i16 {
            .opc = RT::MemOpcode::OFFS16,
            .imm16 = static_cast<uint16_t>(offset),
        });
    } else if (MathUtils::IsNBits(offset, 32)) {
        Encode(segment, RT::M2i32 {
            .opc = RT::MemOpcode::OFFS32,
            .imm32 = static_cast<uint32_t>(offset),
        });
    } else {
        Encode(segment, RT::M2i64 {
            .opc = RT::MemOpcode::OFFS64,
            .imm64 = static_cast<uint64_t>(offset),
        });
    }
}

void MemSpaceEmitter::OffsetReg(IReg reg) {
    Encode(segment, RT::M2xr {
        .opc = RT::MemOpcode::OFFS_REG,
        .xr = RT::XR {
            .imm = 0,
            .r = reg
        },
    });
}

static RT::MemOpcode ComputeLoadAccessKind(Format::LoadAccessKind ldk, RT::MemOpcode start) {
    // This code is heavily rely on the fact that opcodes are ordered
    // in the same order as in the switch here.
    uint8_t delta = 0;
    switch (ldk) {
        case Format::LoadAccessKind::LD_U8:      delta = 0; break;
        case Format::LoadAccessKind::LD_U16:     delta = 1; break;
        case Format::LoadAccessKind::LD_32:      delta = 2; break;
        case Format::LoadAccessKind::LD_S8:      delta = 3; break;
        case Format::LoadAccessKind::LD_S16:     delta = 4; break;
        case Format::LoadAccessKind::LD_F32:     delta = 5; break;
        case Format::LoadAccessKind::LD_F64:     delta = 6; break;
        case Format::LoadAccessKind::LD_64:      delta = 7; break;
        case Format::LoadAccessKind::LD_S32TO64: delta = 8; break;
        case Format::LoadAccessKind::LD_REF:     delta = 9; break;
        default: ASSERTION(false, "unexpected ldk");
    }
    return RT::MemOpcode(start + delta);
}

static RT::MemOpcode ComputeStoreAccessKind(Format::StoreAccessKind stk, RT::MemOpcode start) {
    // This code is heavily rely on the fact that opcodes are ordered
    // in the same order as in the switch here.
    uint8_t delta = 0;
    switch (stk) {
        case Format::StoreAccessKind::ST_8:   delta = 0; break;
        case Format::StoreAccessKind::ST_16:  delta = 1; break;
        case Format::StoreAccessKind::ST_32:  delta = 2; break;
        case Format::StoreAccessKind::ST_64:  delta = 3; break;
        case Format::StoreAccessKind::ST_REF: delta = 4; break;
        case Format::StoreAccessKind::ST_F32: delta = 5; break;
        case Format::StoreAccessKind::ST_F64: delta = 6; break;
        default: ASSERTION(false, "unexpected ldk");
    }
    return RT::MemOpcode(start + delta);
}

void MemSpaceEmitter::LoadObj(Format::LoadAccessKind ldk, RT::Reg dst, IReg base) {
    RT::MemOpcode opc = ComputeLoadAccessKind(ldk, RT::MemOpcode::RLD_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::RLD_END_OPCODE);
    Encode(segment, RT::M2rr {
        .opc = opc,
        .rr = RT::RR {
            .x = dst,
            .y = base
        },
    });
}

void MemSpaceEmitter::StoreObj(Format::StoreAccessKind stk, RT::Reg src, IReg base) {
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::RST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::RST_END_OPCODE);
    Encode(segment, RT::M2rr {
        .opc = opc,
        .rr = RT::RR {
            .x = src,
            .y = base
        },
    });
}

} // namespace Emitter
} // namespace Cbc
