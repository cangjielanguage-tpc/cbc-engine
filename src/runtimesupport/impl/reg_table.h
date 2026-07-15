#pragma once

#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "runtimesupport/runtime.h"
#include "utils/logger.h"
#include "utils/rt_logger.h"

#include <bitset>
#include <cstdint>
#include <functional>

namespace GCSupport {

using Placeholder = uintptr_t*;

class RegistersTable {
    using IReg = Cbc::IReg;

public:
    RegistersTable(Interpretation::Ectype* ectype);

    void VisitAliveRegs(std::bitset<ECTYPE_IREGS_COUNT> aliveRegsMap, std::function<void(Placeholder)> visitor);

    void UpdateRegLocations(Interpretation::NonVolatileRegs savedRegs, Placeholder calleeSavedRegsEnd);

    Placeholder GetRegLocation(IReg reg) { return regLocationMap[reg.Raw()]; }

    void UpdateRegLocation(IReg reg, Placeholder newPlace) { regLocationMap[reg.Raw()] = newPlace; }

private:
    Placeholder regLocationMap[IReg::COUNT];
};

} // namespace GCSupport
