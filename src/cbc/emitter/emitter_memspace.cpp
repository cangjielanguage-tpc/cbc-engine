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

void MemSpaceEmitter::Offset(int32_t offset)
{
    if (offset == 0)
        return;
    if (MathUtils::IsNBitsSigned(offset, 16)) {
        Encode(
            segment,
            RT::M3i16 {
                .opc   = RT::MemOpcode::OFFS16,
                .imm16 = static_cast<uint16_t>(offset),
            }
        );
    } else if (MathUtils::IsNBitsSigned(offset, 32)) {
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

void MemSpaceEmitter::StoreObj(StoreAccessKind stk, Reg src, IReg base)
{
    RT::MemOpcode opc = ComputeStoreAccessKind(stk, RT::MemOpcode::RST_START_OPCODE);
    ASSERT(opc <= RT::MemOpcode::RST_END_OPCODE);
    LoadStore(stk, src, base, opc);
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

void MemSpaceEmitter::StoreFrameImm(StoreAccessKind stk, uint64_t imm)
{
    if (stk == StoreAccessKind::ST_REF) {
        ASSERT(imm == 0);
        return;
    }
    stk           = NormalizeStoreImmKind(stk);
    auto opcStart = ComputeStoreImmStart(stk, RT::MemOpcode::FSTI_START_OPCODE);
    EmitStoreImm(segment, stk, imm, opcStart, RT::MemOpcode::FSTI_END_OPCODE, ImmBuilder {});
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
