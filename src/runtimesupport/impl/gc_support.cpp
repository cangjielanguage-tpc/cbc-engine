#include "gc_support.h"

#include "asm_export.h"
#include "cjnative.h"
#include "engine/statics_manager.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "utils/logger.h"
#include "utils/rt_logger.h"

#include <bitset>

namespace GCSupport {

using namespace Stream;
using placeholder = uintptr_t*;

static void VisitRoot(DYN_RootVisitorT rootVisitor, placeholder ph) {
    RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
        out.PrintFmtLn("visiting %p, value=%p", ph, *ph);
    });
    g_CJNativeInterfaceInstance.visitRootFromInterpreter(rootVisitor, ph);
}

class RegistersTable {
    using IReg = Cbc::IReg;

public:
    RegistersTable(Interpretation::Ectype* ectype): regLocationMap(IReg::COUNT)
    {
        uintptr_t ectypeAddr = reinterpret_cast<uintptr_t>(ectype);
        for (uint32_t regN = 0; regN < IReg::COUNT; regN++) {
            regLocationMap[regN] = reinterpret_cast<placeholder>(ectypeAddr + ECTYPE_IREGS_OFFSET + (regN * ECTYPE_REG_SIZE));
        }
    }

    void VisitAliveRegs(std::bitset<ECTYPE_IREGS_COUNT> aliveRegsMap, DYN_RootVisitorT rootVisitor)
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

    void UpdateRegLocations(std::bitset<ECTYPE_IREGS_COUNT> savedRegsMask, placeholder spillsEnd)
    {
        placeholder spillAddr = spillsEnd;
        for (uint32_t regN = IReg::FIRST_NON_VOL; regN < IReg::COUNT; regN++) {
            if (savedRegsMask.test(regN - IReg::FIRST_NON_VOL)) {
                regLocationMap[regN] = --spillAddr;
            }
        }
    }

private:
    std::vector<placeholder> regLocationMap;
};

void IterateFramesWithState(DYN_CJThreadSpecificDataT threadSpecificData, void (*callback)(DYN_VisitingStateT, void*), void* ctx)
{
    if (threadSpecificData == nullptr) {
        // fiber didn't executed patch code, nothing to do
        return;
    }

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("start scanning frames, thread spec data = %p", threadSpecificData);
    });

    RegistersTable regTable(static_cast<Interpretation::Ectype*>(NOTNULL(threadSpecificData)));
    DYN_VisitingStateT state = &regTable;

    callback(state, ctx);

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("end scanning frames, thread spec data = %p", threadSpecificData);
    });
}

void VisitGCFrameRoots(DYN_VisitingStateT state, DYN_FrameDescT frame_desc, DYN_RootVisitorT rootVisitor)
{
    using namespace Interpretation;
    auto regsLocationTable = reinterpret_cast<RegistersTable*>(state);

    const auto readerOffset = LOCAL_SLOTS_OFFSET + READER_SLOTS_SIZE;

    auto fuh    = *reinterpret_cast<DynamicFunctionHandle**>((uint8_t*)frame_desc.fp - FUH_SLOT_OFFSET);
    auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*)frame_desc.fp - readerOffset);
    auto bc     = NOTNULL(fuh->bytecode.load());

    uint32_t curPos = reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(bc->code.bytecode);

    const ReferenceInfo* refInfo = nullptr;
    for (auto& info : bc->referenceInfos) {
        if (info.rewrittenPos == curPos) {
            refInfo = &info;
            break;
        }
    }

    auto spillsEnd = ((uint8_t*)frame_desc.fp) - readerOffset;
    auto slotsStartAddr = ((uint8_t*)frame_desc.fp) - (readerOffset + bc->frameSize);

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

    for (auto& refSlotOffset : NOTNULL(refInfo)->refSlotOffsets) {
        auto refLocation = reinterpret_cast<placeholder>(slotsStartAddr + refSlotOffset);
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) { out << refSlotOffset << ": "; });
        VisitRoot(rootVisitor, refLocation);
    }

    auto aliveRegsMap = std::bitset<ECTYPE_IREGS_COUNT>(NOTNULL(refInfo)->regMask);
    {
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
            out << "visit alive regs, alive regs: " << aliveRegsMap.to_string().c_str() << endl;
        });

        regsLocationTable->VisitAliveRegs(aliveRegsMap, rootVisitor);
    }

    auto savedRegsMap = std::bitset<ECTYPE_IREGS_COUNT>(bc->savedIRegs);
    {
        RTSupport::Log::gc.Log(Logging::Level::TRACE, [&](Output& out) {
            out << "update regs table, saved regs: " << savedRegsMap.to_string().c_str() << endl;
        });

        regsLocationTable->UpdateRegLocations(savedRegsMap, reinterpret_cast<placeholder>(spillsEnd));
    }

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) {
        out.PrintFmtLn("end visiting frame (fuh=%p)", fuh);
    });
}

void VisitGlobalRoots(DYN_RootVisitorT rootVisitor) {
    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) { out << "start visiting global roots" << endl; });

    auto& engine = Engine::GetEngineInstance();
    Engine::StaticsManager::Of(engine).VisitRefLocations([rootVisitor](Engine::RefLocation* refLocation) {
        VisitRoot(rootVisitor, &refLocation->reference);
    });

    RTSupport::Log::gc.Log(Logging::Level::INFO, [&](Output& out) { out << "end visiting global roots" << endl; });
}

}