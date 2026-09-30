#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "asm_export.h"
#include "isa.h"

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
};

class MoveResolver {
public:
    void Clear();
    void AddMove(Location src, Location dst);
    void Resolve(const std::function<void(Location dst, Location src)>& emit);

    static constexpr auto NIL = Location { -1 };
    static constexpr auto TEMP_IR = Location {IReg::TAIL_REG};
    static constexpr auto TEMP_FR = Location {IReg::VIRT_COUNT + FReg::FR15};

    MoveResolver();

private:
    std::vector<Location> assignments;
};

} // namespace Cbc
