#include "stack_expansion.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "gc_support.h"
#include "interpreter/function_handle.h"
#include "reg_table.h"

namespace StackExpansion {

using namespace GCSupport;
using namespace Interpretation;

// Checks if the received ip is an ip of frame in which stack check is occured.
static bool isTopInterpreterFrame(uintptr_t ip)
{
    uintptr_t start = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_start);
    uintptr_t end   = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_start);
    return start <= ip && ip < end;
}

static std::pair<const GCPositionalInfo*, const StackPtrsPositionalInfo*> FindPositionalInfo(
    ExecBytecodeInfo* bc, uintptr_t pos
)
{
    const GCPositionalInfo* gcPosInfo = nullptr;
    for (const auto& info : bc->gcInfo.positionalInfo) {
        if (info.rewrittenPos == pos) {
            gcPosInfo = &info;
            break;
        }
    }

    const StackPtrsPositionalInfo* stackPtrsPosInfo = nullptr;
    for (const auto& info : bc->stackPtrsInfo.positionalInfo) {
        if (info.rewrittenPos == pos) {
            stackPtrsPosInfo = &info;
        }
    }

    if (!gcPosInfo || !stackPtrsPosInfo) {
        RTSupport::Log::gc.Log(Logging::Level::ERROR, [&](Stream::Output& out) {
            const char* prefix = !gcPosInfo ? "gc" : "stack ptrs";
            out.PrintFmtLn("%s info not found (pos=%lu)", prefix, pos);
        });
    }

    return std::pair(gcPosInfo, stackPtrsPosInfo);
}

void VisitFrameRootsForStackPtrs(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor
)
{
    auto regTable = reinterpret_cast<GCSupport::RegistersTable*>(state);
    auto fuh      = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)frameDesc.fp - FUH_SLOT_OFFSET);
    auto bc       = NOTNULL(fuh->bytecode.load());

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Stream::Output& out) {
        out.PrintFmtLn(
            "start visiting frame for stack ptrs adjusting (fuh=%p, ip=%p, fp=%p)", fuh, frameDesc.ip, frameDesc.fp
        );
    });

    if (isTopInterpreterFrame(reinterpret_cast<uintptr_t>(frameDesc.ip))) {
        auto dumpSize      = ((ECTYPE_IREGS_COUNT * ECTYPE_REG_SIZE) + 15) & ~15;
        auto dumpStartAddr = ((uint8_t*)frameDesc.fp) - (LOCAL_SLOTS_OFFSET + dumpSize);

        Placeholder regAddr = reinterpret_cast<Placeholder>(dumpStartAddr);
        for (int regIdx = 0; regIdx < IReg::COUNT; regIdx++) {
            regTable->UpdateRegLocation(IReg::From(regIdx), regAddr);
            regAddr += ECTYPE_REG_SIZE;
        }

        // TODO get method signature
        // TODO adjust args placeholders (sret, maybe derived ptr, record args) according to signature
    } else {
        auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)frameDesc.fp - READER_SLOT_OFFSET);
        auto curPos = reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(bc->code.bytecode);
        auto slotsStartAddr = ((uint8_t*)frameDesc.fp) - (LOCAL_SLOTS_OFFSET + bc->frameSize);
        auto calleeSavedRegsEnd = ((uint8_t*)frameDesc.fp) - LOCAL_SLOTS_OFFSET;

        auto [gcPosInfo, stackPtrsInfo] = FindPositionalInfo(bc, curPos);

        if (gcPosInfo != nullptr) {
            for (auto& pair : gcPosInfo->mutPairs) {
                auto basePh    = GetResourceLocation(pair.first, slotsStartAddr, regTable);
                auto derivedPh = GetResourceLocation(pair.second, slotsStartAddr, regTable);

                auto baseRef = Value::Reference { .value = *basePh };
                auto locKind = RTSupport::Execution::GetStructLocationKind(baseRef, *derivedPh);
                if (locKind == RTSupport::LOCAL) {
                    VisitMutPair(derivedPtrVisitor, basePh, derivedPh);
                }
            }
        }

        if (stackPtrsInfo != nullptr) {
            for (auto& resource : stackPtrsInfo->resources) {
                auto placeholder = GetResourceLocation(resource, slotsStartAddr, regTable);
                VisitRoot(stackPtrVisitor, placeholder);
            }
        }

        auto savedRegsMap = bc->savedIRegs;
        regTable->UpdateRegLocations(savedRegsMap, reinterpret_cast<Placeholder>(calleeSavedRegsEnd));
    }
}

uint32_t GetFrameSize(DYN_FramePointer fp)
{
    auto fuh = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)fp - FUH_SLOT_OFFSET);
    return fuh->bytecode.load()->frameSize + ADDITIONAL_STACK_SPACE;
}

} // namespace StackExpansion
