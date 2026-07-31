#include "stack_expansion.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa.h"
#include "cjnative.h"
#include "engine/symlevel/definitions.h"
#include "gc_support.h"
#include "interpreter/function_handle.h"
#include "reg_table.h"
#include "resolution/resolution.h"
#include "utils/assertion.h"
#include <cstdint>

namespace StackExpansion {

using namespace GCSupport;
using namespace Interpretation;

// Checks if the received ip is an ip of frame in which stack check is occured.
static bool IsTopInterpreterFrame(uintptr_t ip)
{
    return ip == reinterpret_cast<uintptr_t>(&Asm::engine_after_stack_grow);
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

void needSupportForFrameArgs(uint32_t argIdx) { ASSERT(Resource { .idx = argIdx }.IsReg()); }

// TODO: support for frame arguments
void VisitFrameRootsForStackPtrs(
    DYN_VisitingState state,
    INT_FrameDesc frameDesc,
    DYN_RootVisitor stackPtrVisitor,
    DYN_DerivedPtrVisitor derivedPtrVisitor
)
{
    using namespace RTSupport;

    auto regTable = reinterpret_cast<GCSupport::RegistersTable*>(state);
    auto fuh      = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)frameDesc.fp - FUH_SLOT_OFFSET);
    auto bc       = NOTNULL(fuh->bytecode.load());
    auto slotsStartAddr = ((uint8_t*)frameDesc.fp) - (LOCAL_SLOTS_OFFSET + bc->frameSize);

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Stream::Output& out) {
        out.PrintFmtLn(
            "start visiting frame for stack ptrs adjusting (fuh=%p, ip=%p, fp=%p)", fuh, frameDesc.ip, frameDesc.fp
        );
    });

    // Frame pointer is also a pointer to stack, so it needs to be adjusted.
    VisitRoot(stackPtrVisitor, (Placeholder)frameDesc.fp);

    if (IsTopInterpreterFrame(reinterpret_cast<uintptr_t>(frameDesc.ip))) {
        // Detected frame is in which prologue stack check happens.
        // There are no stack ptr maps for prologue, find roots by abi and signature info.

        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out.PrintFmtLn("found top frame (ip=%p, fp=%p)", frameDesc.ip, frameDesc.fp);
        });

        auto dumpSize      = STACK_OVERFLOW_REGDUMP_SIZE;
        auto dumpStartAddr = ((uint8_t*)frameDesc.fp) - (LOCAL_SLOTS_OFFSET + dumpSize);

        uintptr_t* regs = (uintptr_t*)dumpStartAddr;
        ASSERTION(regs[0] == 0, "must point to IRZ");

        for (int regIdx = 0; regIdx < IReg::COUNT; regIdx++) {
            regTable->UpdateRegLocation(IReg::From(regIdx), &regs[regIdx]);
        }

        auto abiInfo = bc->abiInfo;
        auto resLoc = [slotsStartAddr, regTable](uint32_t idx) {
            return GetResourceLocation(Resource { idx }, slotsStartAddr, regTable);
        };

        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out.PrintFmtLn("rec args: %x, ", abiInfo.adjustableParams);
            out.PrintFmtLn("derived pairs: %x, ", abiInfo.derivedPairs);
        });

        for (uint32_t i = 0; i < IReg::COUNT; i++) {
            if (abiInfo.adjustableParams & (1 << i)) {
                VisitRoot(stackPtrVisitor, resLoc(i));
            }
        }
        for (uint32_t i = 0; i < IReg::COUNT; i++) {
            if (abiInfo.derivedPairs & (1 << i)) {
                auto basePh    = resLoc(i + 1);
                auto derivedPh = resLoc(i);
                auto kind = Execution::GetStructLocationKind(Value::Reference { .value = *basePh }, *derivedPh);

                if (kind == LOCAL) {
                    VisitMutPair(derivedPtrVisitor, basePh, derivedPh);
                }
            }
        }
    } else {
        // One of the caller frames, use stack ptr maps provided by compiler.

        auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)frameDesc.fp - READER_SLOT_OFFSET);
        auto curPos = reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(bc->code.bytecode);
        auto calleeSavedRegsEnd = ((uint8_t*)frameDesc.fp) - LOCAL_SLOTS_OFFSET;

        auto [gcPosInfo, stackPtrsInfo] = FindPositionalInfo(bc, curPos);

        if (gcPosInfo != nullptr) {
            for (auto& pair : gcPosInfo->mutPairs) {
                auto basePh    = GetResourceLocation(pair.first, slotsStartAddr, regTable);
                auto derivedPh = GetResourceLocation(pair.second, slotsStartAddr, regTable);

                auto kind = Execution::GetStructLocationKind(Value::Reference { .value = *basePh }, *derivedPh);
                if (kind == RTSupport::LOCAL) {
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
