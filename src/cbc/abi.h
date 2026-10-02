#pragma once

#include "platforms.h"
#include "move_resolver.h"
#include "utils/span.h"

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

    template <Platform p>
    AbiBuilder(MoveResolver& moves)
        : AbiBuilder(moves, PlatformDescription::FromTraits<p>()) {}

    void Consume(Location loc, Flags flags);
    void ConsumeSret(Location loc);
    void ConsumeReceiverMut(Location loc0, Location loc1);
    void ConsumeReceiver(Location loc);
    void ConsumeFtvars(Location loc);

    template <typename ArgType, typename ArgTypeTraits>
    void Consume(ArgType arg, Location loc) {
        Consume(loc, {
            .isFloat = ArgTypeTraits::IsFloat(arg),
            .isRecord = ArgTypeTraits::IsRecord(arg),
            .isReference = ArgTypeTraits::IsReference(arg),
        });
    }

private:
    MoveResolver& moves;
    PlatformDescription desc;
    int iargIdx;
    int fargIdx;
    int slotIdx;
};

} // namespace Cbc
