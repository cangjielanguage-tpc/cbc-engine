#pragma once

#include "platforms.h"
#include "move_resolver.h"
#include "utils/span.h"
#include "utils/vector.h"

namespace Cbc {

class AbiBuilder {
public:
    struct Flags {
        bool isFloat;
        bool isRecord;
        bool isReference;
    };

    struct PlatformDescription {
        Utils::Span<const IReg> iregHeadArea;
        int iregParamCount;
        Utils::Span<const FReg> fregHeadArea;
        int fregParamCount;
        bool sretShifts;
        int sretIrIdx;

        template <Platform p>
        static PlatformDescription FromTraits() {
            PlatformDescription desc {
                {PlatformTraits<p>::IR_HEAD_AREA, PlatformTraits<p>::IR_PARAM_COUNT},
                PlatformTraits<p>::IR_PARAM_COUNT,
                {PlatformTraits<p>::FR_HEAD_AREA, PlatformTraits<p>::FR_PARAM_COUNT},
                PlatformTraits<p>::FR_PARAM_COUNT,
                PlatformTraits<p>::SRET_SHIFTS,
                0,
            };
            if constexpr (!PlatformTraits<p>::SRET_SHIFTS) {
                desc.sretIrIdx = static_cast<int>(PlatformTraits<p>::SRET_REG);
            }
            return desc;
        }
    };

    AbiBuilder(MoveResolver& moves, const PlatformDescription& desc);

    template <Platform p = HOST_PLATFORM>
    AbiBuilder(MoveResolver& moves)
        : AbiBuilder(moves, PlatformDescription::FromTraits<p>()) {}

    void Clear();
    Location Consume(Location loc, Flags flags);
    void ConsumeSret(Location loc);
    void ConsumeReceiverMut(Location loc0, Location loc1);
    void ConsumeReceiver(Location loc);
    void ConsumeFuncVar(Location loc);
    Location ConsumeOuterTi(Location loc);
    Location ConsumeThisTypeTi(Location loc);

    template <typename ArgType, typename ArgTypeTraits>
    void Consume(ArgType arg, Location loc) {
        Consume(loc, {
            .isFloat = ArgTypeTraits::IsFloat(arg),
            .isRecord = ArgTypeTraits::IsRecord(arg),
            .isReference = ArgTypeTraits::IsReference(arg),
        });
    }

    uint16_t IregStackPtrMask() const { return iregStackPtrMask; }
    uint16_t IregRefMask() const { return iregRefMask; }
    uint16_t FregMask() const { return fregMask; }
    Utils::Span<const int> RefStackSlots() const { return {refStackSlots.Data(), refStackSlots.Size()}; }
    Utils::Span<const int> RecStackSlots() const { return {recStackSlots.Data(), recStackSlots.Size()}; }
    int MaxStackSlot() const { return slotIdx; }

private:
    MoveResolver& moves;
    PlatformDescription desc;
    int iargIdx;
    int fargIdx;
    int slotIdx;
    uint16_t iregStackPtrMask;
    uint16_t iregRefMask;
    uint16_t fregMask;
    Utils::Vector<int> refStackSlots;
    Utils::Vector<int> recStackSlots;
};

} // namespace Cbc
