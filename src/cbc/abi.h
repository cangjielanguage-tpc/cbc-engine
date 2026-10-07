#pragma once

#include "platforms.h"
#include "move_resolver.h"
#include "utils/span.h"

namespace Cbc {

// The kind of a call argument, per platform ABI. Determines which
// register/slot area it occupies and whether the GC must visit it.
enum class ArgKind {
    INT,
    FLOAT,
    REC,
    REF,
};

// Derives ArgKind from a type's float/record/reference traits.
template <typename ArgType, typename ArgTypeTraits>
ArgKind ArgKindOf(const ArgType& arg) {
    if (ArgTypeTraits::IsFloat(arg)) return ArgKind::FLOAT;
    if (ArgTypeTraits::IsRecord(arg)) return ArgKind::REC;
    if (ArgTypeTraits::IsReference(arg)) return ArgKind::REF;
    return ArgKind::INT;
}

// Pure target allocator: hands out the next register or stack slot in
// ABI order for each argument kind. Does not track metadata (masks,
// slot lists) — the consumer derives that from (Location, ArgKind).
class AbiAssigner {
public:
    struct PlatformDescription {
        Utils::Span<const IReg> iregHeadArea;
        Utils::Span<const FReg> fregHeadArea;
        bool sretShifts;
        int sretIrIdx;

        template <Platform p>
        static PlatformDescription FromTraits() {
            PlatformDescription desc {
                {PlatformTraits<p>::IR_HEAD_AREA, PlatformTraits<p>::IR_PARAM_COUNT},
                {PlatformTraits<p>::FR_HEAD_AREA, PlatformTraits<p>::FR_PARAM_COUNT},
                PlatformTraits<p>::SRET_SHIFTS,
                0,
            };
            if constexpr (!PlatformTraits<p>::SRET_SHIFTS) {
                desc.sretIrIdx = static_cast<int>(PlatformTraits<p>::SRET_REG);
            }
            return desc;
        }
    };

    AbiAssigner(const PlatformDescription& desc);

    template <Platform p = HOST_PLATFORM>
    AbiAssigner()
        : AbiAssigner(PlatformDescription::FromTraits<p>()) {}

    Location Consume(ArgKind kind);
    // Consumes the struct-return (sret) argument: the hidden pointer to the
    // return struct. On shift platforms it takes the first int-register slot
    // as a record; on fixed-register platforms a dedicated slot.
    Location ConsumeSret();

    template <typename ArgType, typename ArgTypeTraits>
    Location Consume(ArgType arg) {
        return Consume(ArgKindOf<ArgType, ArgTypeTraits>(arg));
    }

    int MaxStackSlot() const { return slotIdx; }

private:
    PlatformDescription desc;
    int iargIdx;
    int fargIdx;
    int slotIdx;
};

template <typename F>
class TrackedAbiAssigner {
public:
    TrackedAbiAssigner(const AbiAssigner::PlatformDescription& desc, F&& track) : assigner(desc) {}

    template <Platform p = HOST_PLATFORM>
    TrackedAbiAssigner(F&& track)
        : assigner(AbiAssigner::PlatformDescription::FromTraits<p>()) {}

private:
    AbiAssigner assigner;
    F track;
};

} // namespace Cbc
