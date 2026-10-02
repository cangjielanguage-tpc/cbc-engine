#include "abi.h"

namespace Cbc {

AbiBuilder::AbiBuilder(MoveResolver& moves, const PlatformDescription& desc)
    : moves(moves)
    , desc(desc)
    , iargIdx(0)
    , fargIdx(0)
    , slotIdx(0)
{
}

void AbiBuilder::Consume(Location loc, Flags flags)
{
    Location target;
    if (flags.isFloat) {
        if (fargIdx < desc.fregParamCount) {
            target = Location::FReg(desc.fregHeadArea[fargIdx++]);
        } else {
            target = Location::Slot(slotIdx++);
        }
    } else {
        if (iargIdx < desc.iregParamCount) {
            target = Location::IReg(desc.iregHeadArea[iargIdx++]);
        } else {
            target = Location::Slot(slotIdx++);
        }
    }
    moves.AddMove(loc, target);
}

void AbiBuilder::ConsumeSret(Location loc)
{
    Location target;
    if (desc.sretShifts) {
        target = Location::IReg(desc.iregHeadArea[iargIdx++]);
    } else {
        target = Location::IReg(static_cast<IReg::Value>(desc.sretIrIdx));
    }
    moves.AddMove(loc, target);
}

void AbiBuilder::ConsumeReceiverMut(Location loc0, Location loc1)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc0, Location::IReg(desc.iregHeadArea[iargIdx++]));
    } else {
        moves.AddMove(loc0, Location::Slot(slotIdx++));
    }
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc1, Location::IReg(desc.iregHeadArea[iargIdx++]));
    } else {
        moves.AddMove(loc1, Location::Slot(slotIdx++));
    }
}

void AbiBuilder::ConsumeReceiver(Location loc)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc, Location::IReg(desc.iregHeadArea[iargIdx++]));
    } else {
        moves.AddMove(loc, Location::Slot(slotIdx++));
    }
}

void AbiBuilder::ConsumeFtvars(Location loc)
{
    if (iargIdx < desc.iregParamCount) {
        moves.AddMove(loc, Location::IReg(desc.iregHeadArea[iargIdx++]));
    } else {
        moves.AddMove(loc, Location::Slot(slotIdx++));
    }
}

} // namespace Cbc
