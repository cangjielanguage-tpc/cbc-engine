#include "abi.h"

namespace Cbc {

AbiAssigner::AbiAssigner(const PlatformDescription& desc) : desc(desc), iargIdx(0), fargIdx(0), slotIdx(0) {}

Location AbiAssigner::Consume(CbcTypeKind kind)
{
    const bool isFloat = kind == CbcTypeKind::F32 || kind == CbcTypeKind::F64;
    if (isFloat && fargIdx < desc.fregHeadArea.Size()) {
        return Location::FReg(desc.fregHeadArea[fargIdx++]);
    } else if (!isFloat && iargIdx < desc.iregHeadArea.Size()) {
        return Location::IReg(desc.iregHeadArea[iargIdx++]);
    } else {
        return Location::Slot(slotIdx++);
    }
}

Location AbiAssigner::ConsumeVarArg(CbcTypeKind kind)
{
    if (desc.onlyOnStackVarargs) {
        return Location::Slot(slotIdx++);
    }
    return Consume(kind);
}

Location AbiAssigner::ConsumeSret()
{
    if (desc.sretShifts) {
        return Consume(CbcTypeKind::REC);
    }
    return Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
}

} // namespace Cbc
