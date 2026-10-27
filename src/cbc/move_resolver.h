#pragma once

#include <cstdint>

#include "isa.h"
#include "platforms.h"
#include "utils/vector.h"

namespace Cbc {

struct Location {
    int idx;

    static constexpr int REG_COUNT = IReg::VIRT_COUNT + FReg::COUNT;

    constexpr Location() : idx(0) {}

    constexpr Location(int idx) : idx(idx) {}

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
        if (idx < REG_COUNT)
            return FREG;
        return SLOT;
    }

    uint32_t IRegIdx() const { return idx; }

    uint32_t FRegIdx() const { return idx - IReg::VIRT_COUNT; }

    uint32_t SlotIdx() const { return idx - REG_COUNT; }

    static Location IReg(IReg::Value reg) { return Location { static_cast<int>(reg) }; }

    static Location FReg(FReg::Value reg) { return Location { IReg::VIRT_COUNT + static_cast<int>(reg) }; }

    static Location Slot(int slot) { return Location { REG_COUNT + slot }; }

    static constexpr Location Nil() { return Location(-1); }

    bool operator==(const Location& other) const { return idx == other.idx; }

    bool operator!=(const Location& other) const { return idx != other.idx; }

    Location& operator++()
    {
        ++idx;
        return *this;
    }

    operator int() const { return idx; }
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
    virtual void AssignIRegFromAddr(IReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk)      = 0;
    virtual void AssignFRegFromAddr(FReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk)      = 0;
    virtual void AssignSlotFromAddr(int paramSlot, Location addr, int32_t offset, Format::LoadAccessKind ldk) = 0;
};

// Collects src->dst register/slot moves and resolves them into a
// conflict-free instruction sequence. When multiple moves share a
// register, a temp register is used to stage values so no source is
// clobbered before it is read.
class MoveResolver {
public:
    MoveResolver(IReg iRegTemp = PlatformTraits<HOST_PLATFORM>::TR, FReg fRegTemp = FReg::FR15)
        : iRegTemp(iRegTemp),
          fRegTemp(fRegTemp)
    {}

    void Clear();
    void AddMove(Location src, Location dst);
    void AddLoadFromAddr(Location srcAddr, int32_t offset, Format::LoadAccessKind ldk, Location dst);
    // Resolves the collected moves by emitting them through `emitter`.
    // Returns false when a move mixes register kinds (ireg<->freg),
    // which is unsupported.
    bool Resolve(MoveEmitter& emitter);

    static constexpr auto NIL = Location { -1 };

private:
    struct Assignment {
        Location dst;
        Location src;
        int32_t offset;
        Format::LoadAccessKind ldk;
        bool fromAddr;
    };

    Utils::Vector<Assignment> assignments;
    IReg iRegTemp;
    FReg fRegTemp;
};

} // namespace Cbc
