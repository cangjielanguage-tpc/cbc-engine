#pragma once

#include <cstdint>

#include "isa.h"
#include "platforms.h"
#include "utils/vector.h"

namespace Cbc {

struct Location {
    int idx;

    enum Kind : char {
        NIL  = 'n',
        IREG = 'i',
        FREG = 'f',
        SLOT = 's',
    };

    Kind Kind() const
    {
        if (idx < 0)
            return NIL;
        if (idx < IReg::VIRT_COUNT)
            return IREG;
        if (idx < IReg::VIRT_COUNT + FReg::COUNT)
            return FREG;
        return SLOT;
    }

    uint32_t IRegIdx() const { return idx; }

    uint32_t FRegIdx() const { return idx - IReg::VIRT_COUNT; }

    uint32_t SlotIdx() const { return idx - (IReg::VIRT_COUNT + FReg::COUNT); }

    static Location IReg(IReg::Value reg) { return Location { static_cast<int>(reg) }; }

    static Location FReg(FReg::Value reg) { return Location { IReg::VIRT_COUNT + static_cast<int>(reg) }; }

    static Location Slot(int slot) { return Location { IReg::VIRT_COUNT + FReg::COUNT + slot }; }
};

// Abstract sink for resolved moves. Implementations emit the actual
// instructions for each assignment kind. Slot indices are plain ints
// (untyped stack slot for src, param-passing slot for dst).
class MoveEmitter {
public:
    virtual ~MoveEmitter()                                     = default;
    virtual void AssignIReg(IReg dst, IReg src)                = 0;
    virtual void AssignFReg(FReg dst, FReg src)                = 0;
    virtual void AssignParamSlot(int paramSlot, int stackSlot) = 0;
    virtual void AssignFRegFromSlot(FReg dst, int slot)        = 0;
    virtual void AssignIRegFromSlot(IReg dst, int slot)        = 0;
    virtual void AssignSlotFromIReg(int slot, IReg src)        = 0;
    virtual void AssignSlotFromFReg(int slot, FReg src)        = 0;
};

// Collects src→dst register/slot moves and resolves them into a
// conflict-free instruction sequence. When multiple moves share a
// register, a temp register is used to stage values so no source is
// clobbered before it is read.
class MoveResolver {
public:
    void Clear();
    void AddMove(Location src, Location dst);
    // Resolves the collected moves by emitting them through `emitter`.
    // Returns false when a move mixes register kinds (ireg<->freg),
    // which is unsupported.
    bool Resolve(MoveEmitter& emitter);

    static constexpr auto NIL     = Location { -1 };
    static constexpr auto TEMP_FR = FReg::FR15;

    MoveResolver(IReg tempIr = PlatformTraits<HOST_PLATFORM>::TR);

private:
    IReg tempIr;

    struct Assignment {
        Location dst;
        Location src;
    };

    Utils::Vector<Assignment> assignments;
};

} // namespace Cbc
