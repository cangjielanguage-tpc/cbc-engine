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
        bool compactStackSlots;
        bool exhaustFRegsOnHfaSpill;
        CAbi cAbi;

        template <Platform p> static PlatformDescription FromTraits()
        {
            PlatformDescription desc { { PlatformTraits<p>::IR_HEAD_AREA, PlatformTraits<p>::IR_PARAM_COUNT },
                                       { PlatformTraits<p>::FR_HEAD_AREA, PlatformTraits<p>::FR_PARAM_COUNT },
                                       PlatformTraits<p>::SRET_SHIFTS,
                                       0,
                                       PlatformTraits<p>::ONLY_ONSTACK_VARARGS,
                                       PlatformTraits<p>::COMPACT_STACK_SLOTS,
                                       PlatformTraits<p>::EXHAUST_FREGS_ON_HFA_SPILL,
                                       PlatformTraits<p>::C_ABI };
            if constexpr (!PlatformTraits<p>::SRET_SHIFTS) {
                desc.sretIrIdx = static_cast<int>(PlatformTraits<p>::SRET_REG);
            }
            return desc;
        }
    };

    AbiAssigner(const PlatformDescription& desc);

    template <Platform p = HOST_PLATFORM> AbiAssigner() : AbiAssigner(PlatformDescription::FromTraits<p>()) {}

    ArgTarget Consume(CbcTypeKind kind);
    ArgTarget ConsumeVarArg(CbcTypeKind kind);
    // Consumes the struct-return (sret) argument: the hidden pointer to the
    // return struct. On shift platforms it takes the first int-register slot
    // as a record; on fixed-register platforms a dedicated slot.
    ArgTarget ConsumeSret();

    int MaxStackSlot() const { return MaxStackBytes() / STACK_SLOT_SIZE; }

    /// Size in bytes of the whole passing area, rounded up to a whole slot.
    int MaxStackBytes() const { return static_cast<int>(MathUtils::AlignUp(stackBytes, STACK_SLOT_SIZE)); }

    int MaxFRegs() const { return fargIdx; }

    int FreeIRegs() const { return static_cast<int>(desc.iregHeadArea.Size()) - iargIdx; }

    int FreeFRegs() const { return static_cast<int>(desc.fregHeadArea.Size()) - fargIdx; }

    /// Rounds size and start to whole stack slots; returns the first slot index.
    int ConsumeStackArea(uint32_t size)
    {
        return static_cast<int>(
            ConsumeStackBytes(MathUtils::AlignUp(size, STACK_SLOT_SIZE), STACK_SLOT_SIZE) / STACK_SLOT_SIZE
        );
    }

    /// Reserves exactly size bytes at the requested alignment; returns a byte offset.
    uint32_t ConsumeStackBytes(uint32_t size, uint32_t alignment)
    {
        auto base   = AlignStackCursor(alignment);
        stackBytes += static_cast<int>(size);
        return static_cast<uint32_t>(base);
    }

private:
    Location GiveIReg();
    Location GiveFReg();
    Location GiveStackSlot();

    int AlignStackCursor(uint32_t align);
    ArgTarget StackArg(CbcTypeKind kind, uint32_t width);

    PlatformDescription desc;
    int iargIdx;
    int fargIdx;
    int stackBytes;
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

    ArgTarget Consume(CbcTypeKind kind)
    {
        auto target = AbiAssigner::Consume(kind);
        track(target.loc, kind);
        return target;
    }

    ArgTarget ConsumeVarArg(CbcTypeKind kind)
    {
        auto target = AbiAssigner::ConsumeVarArg(kind);
        track(target.loc, kind);
        return target;
    }

    ArgTarget ConsumeSret()
    {
        auto target = AbiAssigner::ConsumeSret();
        track(target.loc, CbcTypeKind::REC);
        return target;
    }

    template <typename ArgType> ArgTarget Consume(ArgType arg)
    {
        ArgTypeTraits<ArgType> traits;
        return Consume(traits.Kind(arg));
    }

    template <typename ArgType> ArgTarget ConsumeVarArg(ArgType arg)
    {
        ArgTypeTraits<ArgType> traits;
        return ConsumeVarArg(traits.Kind(arg));
    }

private:
    F track;
};

} // namespace Cbc
