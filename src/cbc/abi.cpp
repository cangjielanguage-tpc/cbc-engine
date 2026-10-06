#include "abi.h"
#include "cbc/move_resolver.h"

namespace Cbc {

AbiBuilder::AbiBuilder(MoveResolver& moves, const PlatformDescription& desc)
    : moves(moves)
    , desc(desc)
    , iargIdx(0)
    , fargIdx(0)
    , slotIdx(0)
    , iregStackPtrMask(0)
    , iregRefMask(0)
    , fregMask(0)
{
}

void AbiBuilder::Clear()
{
    iargIdx = 0;
    fargIdx = 0;
    slotIdx = 0;
    iregStackPtrMask = 0;
    iregRefMask = 0;
    fregMask = 0;
    refStackSlots.Clear();
    recStackSlots.Clear();
}

Location AbiBuilder::Consume(Location loc, ArgKind kind)
{
    const bool isFloat = kind == ArgKind::FLOAT;
    const bool isRecord = kind == ArgKind::REC;
    const bool isReference = kind == ArgKind::REF;
    Location target;
    if (isFloat && fargIdx < desc.fregHeadArea.Size()) {
        target = Location::FReg(desc.fregHeadArea[fargIdx]);
        fregMask |= (1u << fargIdx);
        fargIdx++;
    } else if (!isFloat && iargIdx < desc.iregHeadArea.Size()) {
        target = Location::IReg(desc.iregHeadArea[iargIdx]);
        if (isRecord) iregStackPtrMask |= (1u << iargIdx);
        if (isReference) iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        target = Location::Slot(slotIdx);
        if (isReference) refStackSlots.PushBack(slotIdx);
        if (isRecord) recStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
    moves.AddMove(loc, target);
    return target;
}

Location AbiBuilder::ConsumeSret(Location loc)
{
    Location target;
    if (desc.sretShifts) {
        target = Consume(loc, ArgKind::REC);
    } else {
        target = Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
        iregStackPtrMask |= (1u << desc.sretIrIdx);
        moves.AddMove(loc, target);
    }
    return target;
}

Location AbiBuilder::ConsumeReceiverMut(Location loc0, Location loc1)
{
    Location target = Consume(loc0, ArgKind::INT);
    Consume(loc1, ArgKind::REF);
    return target;
}

} // namespace Cbc
