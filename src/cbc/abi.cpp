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

Location AbiBuilder::Consume(Location loc, Flags flags)
{
    Location target;
    if (flags.isFloat && fargIdx < desc.fregParamCount) {
        target = Location::FReg(desc.fregHeadArea[fargIdx]);
        fregMask |= (1u << fargIdx);
        fargIdx++;
    } else if (!flags.isFloat && iargIdx < desc.iregParamCount) {
        target = Location::IReg(desc.iregHeadArea[iargIdx]);
        if (flags.isRecord) iregStackPtrMask |= (1u << iargIdx);
        if (flags.isReference) iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        target = Location::Slot(slotIdx);
        if (flags.isReference) refStackSlots.PushBack(slotIdx);
        if (flags.isRecord) recStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
    moves.AddMove(loc, target);
    return target;
}

void AbiBuilder::ConsumeSret(Location loc)
{
    Location target;
    if (desc.sretShifts) {
        Consume(loc, { .isRecord = true });
    } else {
        target = Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
        iregStackPtrMask |= (1u << desc.sretIrIdx);
        moves.AddMove(loc, target);
    }
}

void AbiBuilder::ConsumeReceiverMut(Location loc0, Location loc1)
{
    Consume(loc0, {});
    Consume(loc1, { .isReference = true });
}

void AbiBuilder::ConsumeReceiver(Location loc)
{
    Consume(loc, { .isReference = true });
}

void AbiBuilder::ConsumeFuncVar(Location loc)
{
    Consume(loc, {});
}

Location AbiBuilder::ConsumeOuterTi(Location loc)
{
    return Consume(loc, {});
}

Location AbiBuilder::ConsumeThisTypeTi(Location loc)
{
    return Consume(loc, {});
}

} // namespace Cbc
