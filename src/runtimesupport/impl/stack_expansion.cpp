#include "stack_expansion.h"
#include "asm_trampolines.h"
#include "reg_table.h"

namespace StackExpansion {

// Checks if the received ip is an ip of frame in which stack check is occured.
static bool isTopInterpreterFrame(uintptr_t ip)
{
    uintptr_t start = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_start);
    uintptr_t end   = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_start);
    return start <= ip && ip < end;
}

void VisitFrameRootsForStackPtrs(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor
)
{
    auto regsLocationTable = reinterpret_cast<GCSupport::RegistersTable*>(state);

    if (isTopInterpreterFrame(reinterpret_cast<uintptr_t>(frameDesc.ip))) {
        // TODO get dump of ectype regs
        // TODO get method signature
        // TODO adjust args placeholders (sret, maybe derived ptr, record args) according to signature
    } else {
        // TODO adjust resources according to stack ptr maps
    }
}

} // namespace StackExpansion
