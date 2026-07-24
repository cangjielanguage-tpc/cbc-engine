#include "stack_expansion.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cjnative.h"
#include "engine/symlevel/definitions.h"
#include "gc_support.h"
#include "interpreter/function_handle.h"
#include "reg_table.h"
#include "resolution/resolution.h"

namespace StackExpansion {

using namespace GCSupport;
using namespace Interpretation;

// Checks if the received ip is an ip of frame in which stack check is occured.
static bool isTopInterpreterFrame(uintptr_t ip)
{
    uintptr_t start = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_start);
    uintptr_t end   = reinterpret_cast<uintptr_t>(&Asm::engine_ectype_saving_stub_pc_end);
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

    if (isTopInterpreterFrame(reinterpret_cast<uintptr_t>(frameDesc.ip))) {
        // Detected frame is in which prologue stack check happens.
        // There are no stack ptr maps for prologue, find roots by abi and signature info.

        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
            out.PrintFmtLn("found top frame (ip=%p, fp=%p)", frameDesc.ip, frameDesc.fp);
        });

        auto dumpSize      = ((ECTYPE_IREGS_COUNT * ECTYPE_REG_SIZE) + 15) & ~15;
        auto dumpStartAddr = ((uint8_t*)frameDesc.fp) - (LOCAL_SLOTS_OFFSET + dumpSize);

        uint8_t* regAddr = dumpStartAddr;
        for (int regIdx = 0; regIdx < IReg::COUNT; regIdx++) {
            regTable->UpdateRegLocation(IReg::From(regIdx), reinterpret_cast<Placeholder>(regAddr));
            regAddr += ECTYPE_REG_SIZE;
        }

        Engine::Session session(Engine::GetEngineInstance());
        auto methodDefIdent = fuh->methodDef;
        auto methodDef      = Symlevel::MethodDefinition::Resolve(session, methodDefIdent);
        if (!methodDef.MethodCode().has_value()) {
            return;
        }

        uint32_t argIdx = 1; // IR1

        if (methodDef.GetFlags().Is(Symlevel::MethodFlag::SRET)) {
#if defined(__x86_64__) || defined(_M_X64)
            const uint32_t sretArgIdx = argIdx++;
#elif defined(__aarch64__) || defined(_M_ARM64)
            const uint32_t sretArgIdx = IReg::IR9;
#else
    #error "unsupported platform"
#endif

            RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
                out.PrintFmtLn("found sret (argIdx=%u)", sretArgIdx);
            });

            needSupportForFrameArgs(sretArgIdx);
            auto sretPh = GetResourceLocation(Resource { .idx = sretArgIdx }, slotsStartAddr, regTable);
            VisitRoot(stackPtrVisitor, sretPh);
        }

        if (methodDef.GetFlags().Is(Symlevel::MethodFlag::MUT)) {
            needSupportForFrameArgs(argIdx);
            needSupportForFrameArgs(argIdx + 1);

            auto derivedPh = GetResourceLocation(Resource { .idx = argIdx }, slotsStartAddr, regTable);
            auto basePh    = GetResourceLocation(Resource { .idx = argIdx + 1 }, slotsStartAddr, regTable);
            auto kind      = Execution::GetStructLocationKind(Value::Reference { .value = *basePh }, *derivedPh);

            if (kind == LOCAL) {
                RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
                    out.PrintFmtLn("found derived ptr (argIdx=%u)", argIdx);
                });
                VisitMutPair(derivedPtrVisitor, basePh, derivedPh);

                argIdx += 2; // also skip base ptr
            }
        }

        auto methodSig = Engine::TermManager::Resolve(session, methodDef.Signature());
        for (int subtermIdx = 0; subtermIdx < methodSig.GetLength() - 1 /* without ret type term */; subtermIdx++) {
            if (methodSig.Subterm(subtermIdx).IsRecord()) {
                RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Stream::Output& out) {
                    out.PrintFmtLn("found rec arg (argIdx=%u)", argIdx);
                });

                needSupportForFrameArgs(argIdx);
                auto argPh = GetResourceLocation(Resource { .idx = argIdx }, slotsStartAddr, regTable);
                VisitRoot(stackPtrVisitor, argPh);
            }
            argIdx++;
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
