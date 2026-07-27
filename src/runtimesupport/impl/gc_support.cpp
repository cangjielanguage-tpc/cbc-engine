#include "gc_support.h"

#include "asm_export.h"
#include "cjnative.h"
#include "engine/statics_manager.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/runtime.h"
#include "utils/logger.h"
#include "utils/rt_logger.h"

#include <bitset>
#include <cstdint>

namespace GCSupport {

using namespace Stream;
using Placeholder = uintptr_t*;

static void VisitRoot(DYN_RootVisitor rootVisitor, Placeholder ph)
{
    RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
        out.PrintFmtLn("visiting %p, value=%p", ph, *ph);
    });
    g_CJNativeInterfaceInstance.visitRootFromInterpreter(rootVisitor, ph);
}

static void VisitMutPair(DYN_DerivedPtrVisitor derivedPtrVisitor, Placeholder basePh, Placeholder derivedPh)
{
    RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
        out.PrintFmtLn("visiting derived placeholder=%p, mut pair=(%p, %p)", derivedPh, *basePh, *derivedPh);
    });
    g_CJNativeInterfaceInstance.visitDerivedPtrFromInterpreter(derivedPtrVisitor, basePh, derivedPh);
}

class RegistersTable {
    using IReg = Cbc::IReg;

public:
    RegistersTable(Interpretation::Ectype* ectype)
    {
        uintptr_t ectypeAddr = reinterpret_cast<uintptr_t>(ectype);
        for (uint32_t regN = 0; regN < IReg::COUNT; regN++) {
            IReg reg             = IReg::From(regN);
            regLocationMap[regN] = reinterpret_cast<Placeholder>(ectype->GetIRegLocation(reg));
        }
    }

    void VisitAliveRegs(std::bitset<ECTYPE_IREGS_COUNT> aliveRegsMap, DYN_RootVisitor rootVisitor)
    {
        for (uint32_t regN = 0; regN < IReg::COUNT; regN++) {
            if (aliveRegsMap.test(regN)) {
                RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
                    out << IReg::From(regN).CStr() << ": ";
                });
                VisitRoot(rootVisitor, regLocationMap[regN]);
            }
        }
    }

    void UpdateRegLocations(Interpretation::NonVolatileRegs savedRegs, Placeholder calleeSavedRegsEnd)
    {
        Placeholder addr = calleeSavedRegsEnd;
        while (!savedRegs.IsEmpty()) {
            uint32_t regN        = savedRegs.ExtractReg();
            regLocationMap[regN] = --addr;
        }
    }

    Placeholder GetRegLocation(IReg reg) { return regLocationMap[reg.Raw()]; }

private:
    Placeholder regLocationMap[IReg::COUNT];
};

void IterateFramesWithState(
    DYN_CJThreadSpecificData threadSpecificData, void (*callback)(DYN_VisitingState, void*), void* ctx
)
{
    if (threadSpecificData == nullptr) {
        // fiber didn't executed patch code, nothing to do
        return;
    }

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("start scanning frames, thread spec data = %p", threadSpecificData);
    });

    auto ectype = static_cast<Interpretation::Ectype*>(NOTNULL(threadSpecificData))->Checked();

    RegistersTable regTable(ectype);
    DYN_VisitingState state = &regTable;

    callback(state, ctx);

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("end scanning frames, thread spec data = %p", threadSpecificData);
    });
}

Placeholder GetResourceLocation(Interpretation::Resource resource, uint8_t* slotsStartAddr, RegistersTable* regTable)
{
    if (resource.IsReg()) {
        return regTable->GetRegLocation(resource.AsReg());
    } else {
        return reinterpret_cast<Placeholder>(slotsStartAddr + (resource.AsSlotNum() * 8)); // TODO named constant
    }
}

void VisitGCFrameRoots(
    DYN_VisitingState state,
    INT_FrameDesc frame_desc,
    DYN_RootVisitor rootVisitor,
    std::optional<DYN_DerivedPtrVisitor> derivedPtrVisitorOpt
)
{
    using namespace Interpretation;
    auto regsLocationTable = reinterpret_cast<RegistersTable*>(state);

    const auto localsOffset = LOCAL_SLOTS_OFFSET;

    auto fuh    = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)frame_desc.fp - FUH_SLOT_OFFSET);
    auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)frame_desc.fp - READER_SLOT_OFFSET);
    auto bc     = NOTNULL(fuh->bytecode.load());

    uint32_t curPos = reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(bc->code.bytecode);

    const PositionalInfo* positionalInfo = nullptr;
    for (auto& info : bc->gcInfo.positionalInfo) {
        if (info.rewrittenPos == curPos) {
            positionalInfo = &info;
            break;
        }
    }

    if (!positionalInfo) {
        RTSupport::Log::gc.Log(Logging::Level::ERROR, [&](Output& out) {
            out.PrintFmtLn(
                "cannot translate position (fuh=%p, ip=%p, fp=%p, pos=%p)", fuh, frame_desc.ip, frame_desc.fp, curPos
            );
        });
        return;
    }

    auto calleeSavedRegsEnd = ((uint8_t*)frame_desc.fp) - localsOffset;
    auto slotsStartAddr     = ((uint8_t*)frame_desc.fp) - (localsOffset + bc->frameSize);

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn(
            "start visiting frame (fuh=%p, ip=%p, fp=%p, pos=%p, slots_addr=%p)",
            fuh,
            frame_desc.ip,
            frame_desc.fp,
            curPos,
            slotsStartAddr
        );
    });

    if (derivedPtrVisitorOpt.has_value()) {
        // Heap adjusting is in process
        auto derivedPtrVisitor = *derivedPtrVisitorOpt;
        for (auto& pair : positionalInfo->mutPairs) {
            auto basePh    = GetResourceLocation(pair.first, slotsStartAddr, regsLocationTable);
            auto derivedPh = GetResourceLocation(pair.second, slotsStartAddr, regsLocationTable);

            auto baseRef = Value::Reference { .value = *basePh };
            auto locKind = RTSupport::Execution::GetStructLocationKind(baseRef, *derivedPh);
            if (locKind == RTSupport::HEAP) {
                VisitMutPair(derivedPtrVisitor, basePh, derivedPh); // Adjust derived pointer
            }
        }
    }

    for (auto& refSlotOffset : NOTNULL(positionalInfo)->untypedRefSlotsInfo) {
        auto refLocation = reinterpret_cast<Placeholder>(slotsStartAddr + refSlotOffset);
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) { out << refSlotOffset << ": "; });
        VisitRoot(rootVisitor, refLocation);
    }

    for (auto& typedSlotInfo : bc->gcInfo.typedSlotsInfo) {
        auto typedSlotOffset = typedSlotInfo.first;
        auto typeInfoPtr     = typedSlotInfo.second;

        ASSERTION(!RTSupport::MetaInfo::IsReferenceType(RTSupport::TypeInfo(typeInfoPtr)), "Expected record type");

        Utils::Vector<uint32_t> offsets;
        RTSupport::TypeInfo(typeInfoPtr).VisitReferenceOffsets([&offsets](uint32_t offset) {
            offsets.push_back(offset);
        });

        for (auto& offsetInSlot : offsets) {
            auto refLocation = reinterpret_cast<Placeholder>(slotsStartAddr + typedSlotOffset + offsetInSlot);
            RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
                out.PrintFmtLn(
                    "found reference in typed slot with offset (slot_offset=%u, inner_offset=%u)",
                    typedSlotOffset,
                    offsetInSlot
                );
            });
            VisitRoot(rootVisitor, refLocation);
        }
    }

    auto aliveRegsMap = std::bitset<ECTYPE_IREGS_COUNT>(NOTNULL(positionalInfo)->regMask);
    {
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
            out << "visit alive regs, alive regs: " << aliveRegsMap.to_string().c_str() << endl;
        });

        regsLocationTable->VisitAliveRegs(aliveRegsMap, rootVisitor);
    }

    auto savedRegsMap = bc->savedIRegs;
    {
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
            auto regs     = savedRegsMap;
            uint16_t mask = 0;
            while (!regs.IsEmpty()) {
                mask |= 1 << regs.ExtractReg();
            }
            std::bitset<ECTYPE_IREGS_COUNT> bitset(mask);
            out << "update regs table, saved regs: " << bitset.to_string().c_str() << endl;
        });

        regsLocationTable->UpdateRegLocations(savedRegsMap, reinterpret_cast<Placeholder>(calleeSavedRegsEnd));
    }

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("end visiting frame (fuh=%p)", fuh);
    });
}

void VisitGlobalRoots(DYN_RootVisitor rootVisitor)
{
    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) { out << "start visiting global roots" << endl; });

    auto& engine = Engine::GetEngineInstance();
    auto& sm     = Engine::StaticsManager::Of(engine);

    auto untypedSlotsVisitor = [rootVisitor](Engine::RefLocation* refLocation) {
        VisitRoot(rootVisitor, &refLocation->reference);
    };

    sm.VisitRefLocations(untypedSlotsVisitor);

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) { out << "end visiting global roots" << endl; });
}
} // namespace GCSupport
