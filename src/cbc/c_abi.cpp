#include "c_abi.h"

#include "engine/terms.h"
#include "platforms.h"
#include "resolution/resolution.h"
#include "utils/assertion.h"
#include "utils/misc.h"

#include <algorithm>

namespace Cbc {

static CbcTypeKind PieceKind(uint32_t size, bool isFloat)
{
    if (isFloat) {
        return size == 8 ? CbcTypeKind::F64 : CbcTypeKind::F32;
    }
    switch (size) {
        case 8:  return CbcTypeKind::I64;
        case 4:  return CbcTypeKind::I32;
        case 2:  return CbcTypeKind::U16;
        default: return CbcTypeKind::U8;
    }
}

static CAbiStructPlan ClassifySysV(Utils::Span<const CAbiField> fields, uint32_t size)
{
    CAbiStructPlan plan { CAbiPassBy::EMPTY, {} };
    if (size == 0) {
        return plan;
    }

    if (size > 16) {
        plan.passBy = CAbiPassBy::STACK;
        return plan;
    }

    plan.passBy = CAbiPassBy::REGS;
    for (uint32_t lo = 0; lo < size; lo += 8) {
        uint32_t hi   = std::min(lo + 8, size);
        bool allFloat = true;
        // Check that all fields of current piece are FP.
        for (const auto& f : fields) {
            uint32_t end = f.offset + f.size;
            if (end <= lo || f.offset >= hi) {
                // Field is not in current piece.
                continue;
            }
            ASSERTION(f.offset >= lo && end <= hi, "field is between two pieces");
            allFloat = allFloat && f.isFloat;
        }
        plan.pieces.PushBack(CAbiPiece { lo, PieceKind(hi - lo, allFloat) });
    }
    return plan;
}

static CAbiStructPlan ClassifyAAPCS64(Utils::Span<const CAbiField> fields, uint32_t size)
{
    CAbiStructPlan plan { CAbiPassBy::EMPTY, {} };
    if (size == 0) {
        return plan;
    }

    // HFA/HVA: 1-4 homogeneous floating-point fields.
    bool hfa = fields.Size() >= 1 && fields.Size() <= 4;
    if (hfa) {
        uint8_t elemSize = fields[0].size;
        hfa              = elemSize == 2 || elemSize == 4 || elemSize == 8;
        for (const auto& f : fields) {
            hfa = hfa && f.isFloat && f.size == elemSize;
        }
    }
    if (hfa) {
        plan.passBy = CAbiPassBy::REGS;
        for (const auto& f : fields) {
            // One FP reg for each element.
            plan.pieces.PushBack(CAbiPiece { f.offset, PieceKind(f.size, true) });
        }
        return plan;
    }

    if (size > 16) {
        // Too big, pass by stack area.
        plan.passBy = CAbiPassBy::STACK;
    } else {
        // Pass by integer arg regs.
        plan.passBy = CAbiPassBy::REGS;
        for (uint32_t lo = 0; lo < size; lo += 8) {
            // Divide struct into 8-byte pieces.
            plan.pieces.PushBack(CAbiPiece { lo, PieceKind(std::min<uint32_t>(8, size - lo), false) });
        }
    }
    return plan;
}

CAbiStructPlan ClassifyCAbiStruct(
    Utils::Span<const CAbiField> fields, uint32_t size, bool isVarArg, int freeIRegs, int freeFRegs
)
{
    CAbiStructPlan plan;
    switch (HostTraits::C_ABI) {
        case CAbi::SYSV:    plan = ClassifySysV(fields, size); break;
        case CAbi::AAPCS64: plan = ClassifyAAPCS64(fields, size); break;
        default:            FATAL("unknown C ABI"); return plan;
    }

    if (plan.passBy == CAbiPassBy::REGS) {
        int needIRegs = 0;
        int needFRegs = 0;
        for (const auto& piece : plan.pieces) {
            (IsFloat(piece.kind) ? needFRegs : needIRegs)++;
        }
        if ((isVarArg && HostTraits::ONLY_ONSTACK_VARARGS) || needIRegs > freeIRegs || needFRegs > freeFRegs) {
            plan.passBy = CAbiPassBy::STACK;
            plan.pieces.Clear();
        }
    }
    return plan;
}

// Fills `fields` argument with collected leaf fields info.
bool FlattenCAbiFields(
    Resolution::Resolver& resolver, Engine::Term term, uint32_t base, Utils::Vector<CAbiField>& fields
)
{
    UNWRAP_OPT_B(layout, resolver.fieldManager->GetLayout(term));
    for (const auto& entry : layout->fields) {
        UNWRAP_OPT_B(offset, entry.offset);
        offset += base;
        if (entry.fieldType.IsRecord()) {
            if (!FlattenCAbiFields(resolver, entry.fieldType, offset, fields)) {
                return false;
            }
        } else {
            UNWRAP_OPT_B(size, resolver.fieldManager->GetFlatSize(entry.fieldType));
            fields.PushBack(CAbiField { offset, static_cast<uint8_t>(size), entry.fieldType.IsFloat() });
        }
    }
    return true;
}

} // namespace Cbc
