#pragma once

#include "move_resolver.h"
#include "platforms.h"
#include "utils/math.h"
#include "utils/span.h"

#include "frame.h"

namespace Cbc {

template <typename ArgType> struct ArgTypeTraits {
    CbcTypeKind Kind(const ArgType& t);
};

// Pure target allocator: hands out the next register or stack slot in
// ABI order for each argument kind. Does not track metadata (masks,
// slot lists) — the consumer derives that from (Location, CbcTypeKind).
class AbiAssigner {
public:
    struct PlatformDescription {
        Utils::Span<const IReg> iregHeadArea;
        Utils::Span<const FReg> fregHeadArea;
        bool sretShifts;
        int sretIrIdx;
        bool onlyOnStackVarargs;

        template <Platform p> static PlatformDescription FromTraits()
        {
            PlatformDescription desc { { PlatformTraits<p>::IR_HEAD_AREA, PlatformTraits<p>::IR_PARAM_COUNT },
                                       { PlatformTraits<p>::FR_HEAD_AREA, PlatformTraits<p>::FR_PARAM_COUNT },
                                       PlatformTraits<p>::SRET_SHIFTS,
                                       0,
                                       PlatformTraits<p>::ONLY_ONSTACK_VARARGS };
            if constexpr (!PlatformTraits<p>::SRET_SHIFTS) {
                desc.sretIrIdx = static_cast<int>(PlatformTraits<p>::SRET_REG);
            }
            return desc;
        }
    };

    AbiAssigner(const PlatformDescription& desc);

    template <Platform p = HOST_PLATFORM> AbiAssigner() : AbiAssigner(PlatformDescription::FromTraits<p>()) {}

    Location Consume(CbcTypeKind kind);
    Location ConsumeVarArg(CbcTypeKind kind);
    // Consumes the struct-return (sret) argument: the hidden pointer to the
    // return struct. On shift platforms it takes the first int-register slot
    // as a record; on fixed-register platforms a dedicated slot.
    Location ConsumeSret();

    int MaxStackSlot() const { return slotIdx; }

    int MaxFRegs() const { return fargIdx; }

    int FreeIRegs() const { return static_cast<int>(desc.iregHeadArea.Size()) - iargIdx; }

    int FreeFRegs() const { return static_cast<int>(desc.fregHeadArea.Size()) - fargIdx; }

    int ConsumeStackArea(uint32_t size)
    {
        int base  = slotIdx;
        slotIdx  += static_cast<int>(MathUtils::AlignUp(size, STACK_SLOT_SIZE) / STACK_SLOT_SIZE);
        return base;
    }

private:
    Location GiveIReg();
    Location GiveFReg();
    Location GiveStackSlot();

    PlatformDescription desc;
    int iargIdx;
    int fargIdx;
    int slotIdx;
};

template <typename F> class TrackedAbiAssigner : public AbiAssigner {
public:
    TrackedAbiAssigner(const AbiAssigner::PlatformDescription& desc, F&& track)
        : AbiAssigner(desc),
          track(std::move(track))
    {}

    template <Platform p = HOST_PLATFORM>
    TrackedAbiAssigner(F&& track)
        : AbiAssigner(AbiAssigner::PlatformDescription::FromTraits<p>()),
          track(std::move(track))
    {}

    Location Consume(CbcTypeKind kind)
    {
        auto target = AbiAssigner::Consume(kind);
        track(target, kind);
        return target;
    }

    Location ConsumeVarArg(CbcTypeKind kind)
    {
        auto target = AbiAssigner::ConsumeVarArg(kind);
        track(target, kind);
        return target;
    }

    Location ConsumeSret()
    {
        auto target = AbiAssigner::ConsumeSret();
        track(target, CbcTypeKind::REC);
        return target;
    }

    template <typename ArgType> Location Consume(ArgType arg)
    {
        ArgTypeTraits<ArgType> traits;
        return Consume(traits.Kind(arg));
    }

    template <typename ArgType> Location ConsumeVarArg(ArgType arg)
    {
        ArgTypeTraits<ArgType> traits;
        return ConsumeVarArg(traits.Kind(arg));
    }

private:
    F track;
};

} // namespace Cbc
