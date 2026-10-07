#include "abi.h"

namespace Cbc {

AbiAssigner::AbiAssigner(const PlatformDescription& desc)
    : desc(desc)
    , iargIdx(0)
    , fargIdx(0)
    , slotIdx(0)
{
}

void AbiAssigner::Clear()
{
    iargIdx = 0;
    fargIdx = 0;
    slotIdx = 0;
}

Location AbiAssigner::Consume(ArgKind kind)
{
    const bool isFloat = kind == ArgKind::FLOAT;
    Location target;
    if (isFloat && fargIdx < desc.fregHeadArea.Size()) {
        target = Location::FReg(desc.fregHeadArea[fargIdx]);
        fargIdx++;
    } else if (!isFloat && iargIdx < desc.iregHeadArea.Size()) {
        target = Location::IReg(desc.iregHeadArea[iargIdx]);
        iargIdx++;
    } else {
        target = Location::Slot(slotIdx);
        slotIdx++;
    }
    return target;
}

Location AbiAssigner::ConsumeSret()
{
    if (desc.sretShifts) {
        return Consume(ArgKind::REC);
    }
    return Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
}

} // namespace Cbc
