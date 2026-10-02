#include "abi.h"

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

void AbiBuilder::Consume(Location loc, Flags flags)
{
    Location target;
    if (flags.isFloat) {
        if (fargIdx < desc.fregParamCount) {
            target = Location::FReg(desc.fregHeadArea[fargIdx]);
            fregMask |= (1u << fargIdx);
            fargIdx++;
        } else {
            target = Location::Slot(slotIdx);
            if (flags.isReference) refStackSlots.PushBack(slotIdx);
            if (flags.isRecord) recStackSlots.PushBack(slotIdx);
            slotIdx++;
        }
    } else {
        if (iargIdx < desc.iregParamCount) {
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
    }
    moves.AddMove(loc, target);
}

void AbiBuilder::ConsumeSret(Location loc)
{
    Location target;
    if (desc.sretShifts) {
        target = Location::IReg(desc.iregHeadArea[iargIdx]);
        iregStackPtrMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        target = Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
    }
    moves.AddMove(loc, target);
}

void AbiBuilder::ConsumeReceiverMut(Location loc0, Location loc1)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc0, Location::IReg(desc.iregHeadArea[iargIdx]));
        iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        moves.AddMove(loc0, Location::Slot(slotIdx));
        refStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc1, Location::IReg(desc.iregHeadArea[iargIdx]));
        iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        moves.AddMove(loc1, Location::Slot(slotIdx));
        refStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
}

void AbiBuilder::ConsumeReceiver(Location loc)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc, Location::IReg(desc.iregHeadArea[iargIdx]));
        iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        moves.AddMove(loc, Location::Slot(slotIdx));
        refStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
}

void AbiBuilder::ConsumeFtvars(Location loc)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc, Location::IReg(desc.iregHeadArea[iargIdx]));
        iregRefMask |= (1u << iargIdx);
        iargIdx++;
    } else {
        moves.AddMove(loc, Location::Slot(slotIdx));
        refStackSlots.PushBack(slotIdx);
        slotIdx++;
    }
}

} // namespace Cbc
