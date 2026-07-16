#include "reg_table.h"

namespace GCSupport {

RegistersTable::RegistersTable(Interpretation::Ectype* ectype)
{
    uintptr_t ectypeAddr = reinterpret_cast<uintptr_t>(ectype);
    for (uint32_t regN = 0; regN < IReg::COUNT; regN++) {
        IReg reg             = IReg::From(regN);
        regLocationMap[regN] = reinterpret_cast<Placeholder>(ectype->GetIRegLocation(reg));
    }
}

void RegistersTable::VisitAliveRegs(
    std::bitset<ECTYPE_IREGS_COUNT> aliveRegsMap, std::function<void(Placeholder)> visitor
)
{
    for (uint32_t regN = 0; regN < IReg::COUNT; regN++) {
        if (aliveRegsMap.test(regN)) {
            RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
                out << IReg::From(regN).CStr() << ": ";
            });
            visitor(regLocationMap[regN]);
        }
    }
}

void RegistersTable::UpdateRegLocations(Interpretation::NonVolatileRegs savedRegs, Placeholder calleeSavedRegsEnd)
{
    Placeholder addr = calleeSavedRegsEnd;
    while (!savedRegs.IsEmpty()) {
        uint32_t regN        = savedRegs.ExtractReg();
        regLocationMap[regN] = --addr;
    }
}

Placeholder GetResourceLocation(Interpretation::Resource resource, uint8_t* slotsStartAddr, RegistersTable* regTable)
{
    if (resource.IsReg()) {
        return regTable->GetRegLocation(resource.AsReg());
    } else {
        return reinterpret_cast<Placeholder>(slotsStartAddr + (resource.AsSlotNum() * 8)); // TODO named constant
    }
}

} // namespace GCSupport
