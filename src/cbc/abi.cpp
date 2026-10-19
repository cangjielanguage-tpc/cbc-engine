#include "abi.h"

namespace Cbc {

AbiAssigner::AbiAssigner(const PlatformDescription& desc) : desc(desc), iargIdx(0), fargIdx(0), slotIdx(0) {}

Location AbiAssigner::GiveIReg() { return Location::IReg(desc.iregHeadArea[iargIdx++]); }

Location AbiAssigner::GiveFReg() { return Location::FReg(desc.fregHeadArea[fargIdx++]); }

Location AbiAssigner::GiveStackSlot() { return Location::Slot(slotIdx++); }

Location AbiAssigner::Consume(ArgKind kind)
{
    const bool isFloat = kind == ArgKind::FLOAT;
    if (isFloat && fargIdx < desc.fregHeadArea.Size()) {
        return GiveFReg();
    } else if (!isFloat && iargIdx < desc.iregHeadArea.Size()) {
        return GiveIReg();
    } else {
        return GiveStackSlot();
    }
}

Location AbiAssigner::ConsumeVarArg(ArgKind kind)
{
    if (desc.onlyOnStackVarargs) {
        return GiveStackSlot();
    }
    return Consume(kind);
}

Location AbiAssigner::ConsumeSret()
{
    if (desc.sretShifts) {
        return Consume(ArgKind::REC);
    }
    return Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
}

} // namespace Cbc
