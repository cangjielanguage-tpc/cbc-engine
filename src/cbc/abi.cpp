#include "abi.h"

namespace Cbc {

AbiAssigner::AbiAssigner(const PlatformDescription& desc) : desc(desc), iargIdx(0), fargIdx(0), stackBytes(0) {}

static uint32_t StackArgSize(CbcTypeKind kind, bool compact)
{
    if (!compact) {
        return STACK_SLOT_SIZE;
    }
    switch (kind) {
        case CbcTypeKind::U8:
        case CbcTypeKind::I8:
        case CbcTypeKind::BOOL: return 1;
        case CbcTypeKind::F16:
        case CbcTypeKind::U16:
        case CbcTypeKind::I16:  return 2;
        case CbcTypeKind::U32:
        case CbcTypeKind::I32:
        case CbcTypeKind::F32:  return 4;
        default:                return STACK_SLOT_SIZE;
    }
}

ArgTarget AbiAssigner::StackArg(CbcTypeKind kind, uint32_t width)
{
    int offset  = AlignStackCursor(width);
    stackBytes += static_cast<int>(width);
    auto stk     = kind == CbcTypeKind::REC || kind == CbcTypeKind::REF ? Format::StoreAccessKind::ST_64 : Stk(kind);
    return ArgTarget::Stack(offset / STACK_SLOT_SIZE, stk, static_cast<uint32_t>(offset % STACK_SLOT_SIZE));
}

int AbiAssigner::AlignStackCursor(uint32_t align)
{
    stackBytes = static_cast<int>(MathUtils::AlignUp(static_cast<uint32_t>(stackBytes), align));
    return stackBytes;
}

ArgTarget AbiAssigner::Consume(CbcTypeKind kind)
{
    if (IsFloat(kind) && fargIdx < desc.fregHeadArea.Size()) {
        return ArgTarget::Reg(Location::FReg(desc.fregHeadArea[fargIdx++]));
    }
    if (!IsFloat(kind) && iargIdx < desc.iregHeadArea.Size()) {
        return ArgTarget::Reg(Location::IReg(desc.iregHeadArea[iargIdx++]));
    }
    return StackArg(kind, StackArgSize(kind, desc.compactStackSlots));
}

ArgTarget AbiAssigner::ConsumeVarArg(CbcTypeKind kind)
{
    if (desc.onlyOnStackVarargs) {
        // Varargs always occupy a whole slot, never compacted.
        return StackArg(kind, STACK_SLOT_SIZE);
    }
    return Consume(kind);
}

ArgTarget AbiAssigner::ConsumeSret()
{
    if (desc.sretShifts) {
        return Consume(CbcTypeKind::REC);
    }
    return ArgTarget::Reg(Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx)));
}

} // namespace Cbc
