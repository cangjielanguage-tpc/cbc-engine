#include <cstring>
#include <utility>

#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "emitter.h"
#include "encoding_rt.h"
#include "runtimesupport/runtime.h"
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
    if (offset == 0)
        return;
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

void MemSpaceEmitter::OffsetRegIdx(IReg reg, uint64_t size)
{
    Encode(
        segment,
        RT::M10xri64 {
            .opc   = RT::MemOpcode::OFFS_REG_IDX64,
            .xr    = XR { .imm = 0, .r = reg },
            .imm64 = Format::Imm64 { .imm = size },
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
        case LoadAccessKind::LD_LEA:     delta = 10; break;
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

void MemSpaceEmitter::WriteStructFieldObj(IReg src, IReg base, RTSupport::TypeInfo structTypeInfo)
{
    RT::MStructFieldOp command = { .opc = RT::MemOpcode::R_WRITE_STRUCT, .rr = { src, base }, .ti = structTypeInfo };
    Encode(segment, command);
}

void MemSpaceEmitter::ReadStructFieldObj(IReg dst, IReg base, RTSupport::TypeInfo structTypeInfo)
{
    RT::MStructFieldOp command = { .opc = RT::MemOpcode::R_READ_STRUCT, .rr = { dst, base }, .ti = structTypeInfo };
    Encode(segment, command);
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

void MemSpaceEmitter::LoadDerived(LoadAccessKind ldk, Reg dst, IReg base, IReg derived)
{
    RT::MemOpcode opc = ComputeLoadAccessKind(ldk, RT::MemOpcode::DLD_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::DLD_END_OPCODE);
    LoadStore(ldk, dst, base, opc);
    Encode(
        segment,
        RT::M3xrrr {
            .opc = opc,
            .xr  = Format::XR { .imm = 0, .r = dst },
            .rr  = Format::RR { .x = base, .y = derived },
        }
    );
}

void MemSpaceEmitter::CopyRec(Reg from, Reg to, RTSupport::TypeInfo ti, RT::MemOpcode opc)
{
    Encode(
        segment,
        RT::MStructFieldOp {
            .opc = opc,
            .rr  = RR { .x = from, .y = to },
            .ti  = ti,
        }
    );
}

void MemSpaceEmitter::CopyRecFromObj(Reg from, Reg to, RTSupport::TypeInfo ti)
{
    CopyRec(from, to, ti, RT::MemOpcode::COPY_REC_FROM_OBJ);
}

void MemSpaceEmitter::CopyRecFromRec(Reg from, Reg to, RTSupport::TypeInfo ti)
{
    CopyRec(from, to, ti, RT::MemOpcode::COPY_REC_FROM_REC);
}

void MemSpaceEmitter::CopyDerivedFromRec(Reg base, Reg derived, Reg to, RTSupport::TypeInfo ti)
{
    Encode(
        segment,
        RT::CopyDerived {
            .opc   = RT::MemOpcode::COPY_REC_FROM_DERIVED,
            .rr    = RR { .x = base, .y = derived },
            .field = RR { .x = to, .y = 0 },
        }
    );
}

void MemSpaceEmitter::CopyObjToRec(Reg from, Reg to, RTSupport::TypeInfo ti)
{
    CopyRec(from, to, ti, RT::MemOpcode::COPY_REC_TO_OBJ);
}

void MemSpaceEmitter::CopyRecToRec(Reg from, Reg to, RTSupport::TypeInfo ti)
{
    CopyRec(from, to, ti, RT::MemOpcode::COPY_REC_TO_REC);
}

void MemSpaceEmitter::CopyDerivedToRec(Reg base, Reg derived, Reg from, RTSupport::TypeInfo ti)
{
    Encode(
        segment,
        RT::CopyDerived {
            .opc   = RT::MemOpcode::COPY_REC_TO_DERIVED,
            .rr    = RR { .x = base, .y = derived },
            .field = RR { .x = from, .y = 0 },
        }
    );
}

void MemSpaceEmitter::StoreDerived(StoreAccessKind stk, Reg src, IReg base, IReg derived)
{
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::DST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::DST_END_OPCODE);
    Encode(
        segment,
        RT::M3xrrr {
            .opc = opc,
            .xr  = Format::XR { .imm = 0, .r = src },
            .rr  = Format::RR { .x = base, .y = derived },
        }
    );
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

static StoreAccessKind NormalizeStoreImmKind(StoreAccessKind stk)
{
    switch (stk) {
        case StoreAccessKind::ST_REF: FATAL("Unexpected store access kind: %s", stk.ToStr().data()); return stk;
        case StoreAccessKind::ST_F32: return StoreAccessKind::ST_32;
        case StoreAccessKind::ST_F64: return StoreAccessKind::ST_64;
        default:                      return stk;
    }
}

struct ImmBuilder {
    auto I8(RT::MemOpcode opc, uint8_t v) const { return RT::M2i8 { opc, v }; }

    auto I16(RT::MemOpcode opc, uint16_t v) const { return RT::M3i16 { opc, v }; }

    auto I32(RT::MemOpcode opc, uint32_t v) const { return RT::M5i32 { opc, v }; }

    auto I64(RT::MemOpcode opc, uint64_t v) const { return RT::M9i64 { opc, v }; }
};

struct XRImmBuilder {
    Format::XR xr;

    auto I8(RT::MemOpcode opc, uint8_t v) const { return RT::M3xri8 { opc, xr, { v } }; }

    auto I16(RT::MemOpcode opc, uint16_t v) const { return RT::M4xri16 { opc, xr, { v } }; }

    auto I32(RT::MemOpcode opc, uint32_t v) const { return RT::M6xri32 { opc, xr, { v } }; }

    auto I64(RT::MemOpcode opc, uint64_t v) const { return RT::M10xri64 { opc, xr, { .imm = v } }; }
};

struct RRImmBuilder {
    Format::RR rr;

    auto I8(RT::MemOpcode opc, uint8_t v) const { return RT::M3rri8 { opc, rr, { v } }; }

    auto I16(RT::MemOpcode opc, uint16_t v) const { return RT::M4rri16 { opc, rr, { v } }; }

    auto I32(RT::MemOpcode opc, uint32_t v) const { return RT::M6rri32 { opc, rr, { v } }; }

    auto I64(RT::MemOpcode opc, uint64_t v) const { return RT::M10rri64 { opc, rr, { .imm = v } }; }
};

template <typename Builder>
static void EmitStoreImm(
    Segment& seg,
    StoreAccessKind stk,
    uint64_t imm,
    RT::MemOpcode opcStart,
    RT::MemOpcode endOpcode,
    const Builder& builder
)
{
    if (MathUtils::IsNBitsSigned(imm, 8) || stk == StoreAccessKind::ST_8) {
        ASSERT(opcStart <= endOpcode);
        Encode(seg, builder.I8(opcStart, static_cast<uint8_t>(imm)));
    } else if (MathUtils::IsNBitsSigned(imm, 16) || stk == StoreAccessKind::ST_16) {
        auto opc = RT::MemOpcode(opcStart + 1);
        ASSERT(opc <= endOpcode);
        Encode(seg, builder.I16(opc, static_cast<uint16_t>(imm)));
    } else if (MathUtils::IsNBitsSigned(imm, 32) || stk == StoreAccessKind::ST_32) {
        auto opc = RT::MemOpcode(opcStart + 2);
        ASSERT(opc <= endOpcode);
        Encode(seg, builder.I32(opc, static_cast<uint32_t>(imm)));
    } else {
        auto opc = RT::MemOpcode(opcStart + 3);
        ASSERT(opc <= endOpcode);
        Encode(seg, builder.I64(opc, imm));
    }
}

void MemSpaceEmitter::StoreFrameImm(StoreAccessKind stk, uint64_t imm)
{
    if (stk == StoreAccessKind::ST_REF) {
        ASSERT(imm == 0);
        StoreFrame(stk, IReg::IRZ);
        return;
    }
    stk           = NormalizeStoreImmKind(stk);
    auto opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::FSTI_START_OPCODE);
    EmitStoreImm(segment, stk, imm, opcStart, RT::MemOpcode::FSTI_END_OPCODE, ImmBuilder {});
}

void MemSpaceEmitter::StoreRecImm(StoreAccessKind stk, Reg base, uint64_t imm)
{
    stk           = NormalizeStoreImmKind(stk);
    auto opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::SSTI_START_OPCODE);
    EmitStoreImm(
        segment,
        stk,
        imm,
        opcStart,
        RT::MemOpcode::SSTI_END_OPCODE,
        XRImmBuilder { Format::XR { .imm = 0, .r = base.IR() } }
    );
}

void MemSpaceEmitter::StoreDerivedImm(StoreAccessKind stk, IReg base, IReg derived, uint64_t imm)
{
    stk           = NormalizeStoreImmKind(stk);
    auto opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::DSTI_START_OPCODE);
    EmitStoreImm(
        segment,
        stk,
        imm,
        opcStart,
        RT::MemOpcode::DSTI_END_OPCODE,
        RRImmBuilder { Format::RR { .x = base, .y = derived } }
    );
}

void MemSpaceEmitter::StoreObjImm(StoreAccessKind stk, Reg base, uint64_t imm)
{
    stk           = NormalizeStoreImmKind(stk);
    auto opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::RSTI_START_OPCODE);
    EmitStoreImm(
        segment,
        stk,
        imm,
        opcStart,
        RT::MemOpcode::RSTI_END_OPCODE,
        XRImmBuilder { Format::XR { .imm = 0, .r = base.IR() } }
    );
}

void MemSpaceEmitter::LoadGeneric(IReg dst, IReg base, IReg typeInfo)
{
    auto command = RT::M3rrrr { .opc = RT::MemOpcode::DLD_GENERIC, .rr1 = { base, typeInfo }, .rr2 = { dst, base } };
    Encode(segment, command);
}

void MemSpaceEmitter::StoreGeneric(IReg src, IReg base, IReg typeInfo)
{
    auto command = RT::M3rrrr { .opc = RT::MemOpcode::DST_GENERIC, .rr1 = { base, typeInfo }, .rr2 = { src, base } };
    Encode(segment, command);
}

void MemSpaceEmitter::LoadDerivedGeneric(IReg dst, IReg base, IReg derived, IReg typeInfo)
{
    auto command = RT::M3rrrr { .opc = RT::MemOpcode::DLD_GENERIC, .rr1 = { derived, typeInfo }, .rr2 = { dst, base } };
    Encode(segment, command);
}

void MemSpaceEmitter::StoreDerivedGeneric(IReg src, IReg base, IReg derived, IReg typeInfo)
{
    auto command = RT::M3rrrr { .opc = RT::MemOpcode::DST_GENERIC, .rr1 = { derived, typeInfo }, .rr2 = { src, base } };
    Encode(segment, command);
}

void MemSpaceEmitter::GenericField(int ordinal, IReg typeInfo)
{
    auto command = RT::M6rri32 {
        .opc   = RT::MemOpcode::GENERIC_FIELD,
        .rr    = { typeInfo, typeInfo },
        .imm32 = { (uint32_t)ordinal },
    };
    Encode(segment, command);
}

} // namespace Emitter
} // namespace Cbc
