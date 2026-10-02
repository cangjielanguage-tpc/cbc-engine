#pragma once

#include <cstdint>

#include "platforms.h"
#include "isa.h"
#include "utils/vector.h"
#include "utils/function.h"

namespace Cbc {

struct Location {
    int idx;

    enum Kind : char {
        NIL = 'n',
        IREG = 'i',
        FREG = 'f',
        SLOT = 's',
    };

    Kind Kind() const
    {
        if (idx < 0) return NIL;
        if (idx < IReg::VIRT_COUNT) return IREG;
        if (idx < IReg::VIRT_COUNT + FReg::COUNT) return FREG;
        return SLOT;
    }

    uint32_t IRegIdx() const { return idx; }
    uint32_t FRegIdx() const { return idx - IReg::VIRT_COUNT; }
    uint32_t SlotIdx() const { return idx - (IReg::VIRT_COUNT + FReg::COUNT); }

    static Location IReg(IReg::Value reg) { return Location {static_cast<int>(reg)}; }
    static Location FReg(FReg::Value reg) { return Location {IReg::VIRT_COUNT + static_cast<int>(reg)}; }
    static Location Slot(int slot) { return Location {IReg::VIRT_COUNT + FReg::COUNT + slot}; }
};

class MoveResolver {
public:
    void Clear();
    void AddMove(Location src, Location dst);
    void Resolve(const Utils::Function<void(Location dst, Location src)>& emit);

    static constexpr auto NIL     = Location { -1 };
    static constexpr auto TEMP_FR = Location {IReg::VIRT_COUNT + FReg::FR15};

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
