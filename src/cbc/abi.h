#pragma once

#include "platforms.h"
#include "move_resolver.h"
#include "utils/span.h"
#include "utils/vector.h"

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

// Assigns call arguments to registers or stack slots per platform ABI.
// Integer args fill the platform's integer-register head area in order;
// overflow goes to stack slots. Same for float args. Stack slots that
// hold references or records are recorded so the GC can visit them at
// state points. Frame size is derived from the highest allocated slot.
class AbiBuilder {
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

    AbiBuilder(MoveResolver& moves, const PlatformDescription& desc);

    template <Platform p = HOST_PLATFORM>
    AbiBuilder(MoveResolver& moves)
        : AbiBuilder(moves, PlatformDescription::FromTraits<p>()) {}

    void Clear();
    Location Consume(Location loc, ArgKind kind);
    // Consumes the struct-return (sret) argument: the hidden pointer to the
    // return struct. On shift platforms it takes the first int-register slot
    // as a record; on fixed-register platforms a dedicated slot.
    Location ConsumeSret(Location loc);
    Location ConsumeReceiverMut(Location loc0, Location loc1);

    template <typename ArgType, typename ArgTypeTraits>
    Location Consume(ArgType arg, Location loc) {
        return Consume(loc, ArgKindOf<ArgType, ArgTypeTraits>(arg));
    }

    uint16_t IregStackPtrMask() const { return iregStackPtrMask; }
    uint16_t IregRefMask() const { return iregRefMask; }
    uint16_t FregMask() const { return fregMask; }
    Utils::Span<uint32_t const> RefStackSlots() const { return {refStackSlots.Data(), refStackSlots.Size()}; }
    Utils::Span<uint32_t const> RecStackSlots() const { return {recStackSlots.Data(), recStackSlots.Size()}; }
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
    Utils::Vector<uint32_t> refStackSlots;
    Utils::Vector<uint32_t> recStackSlots;
};

} // namespace Cbc
