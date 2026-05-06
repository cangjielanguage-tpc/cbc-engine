#include "runtimesupport/impl/entrypoint.h"

#include <filesystem>
#include <mutex>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "engine/statics_manager.h"
#include "engine/symlevel/io/filesystem.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "runtimesupport/impl/rt_syms.h"
#include "utils/logger.h"

DYN_CJNativeInterfaceT g_CJNativeInterfaceInstance;

static std::mutex g_InitializationGuard;
static bool g_Initialized;
static bool g_OptionsInitialized;

static void InitEnvOpts() {
    std::lock_guard guard(g_InitializationGuard);
    if (!g_OptionsInitialized) {
        Engine::InitEnvOptions();
        g_OptionsInitialized = true;
    }
}

/// Initialize engine from launcher.
static void EnsureEngineInitialized()
{
    InitEnvOpts();
    std::lock_guard guard(g_InitializationGuard);
    if (g_Initialized) {
        return;
    }

    Engine::InitEnvOptions();
    Engine::Loader loader;
    loader.Load(IO::OpenFile(std::filesystem::path(g_mainCbc)), g_mainCbc);
    loader.Build();
    g_Initialized = true;
}

static void FiberStart(DYN_CJThreadSpecificDataT* data) { /* no-op */ }

static void FiberDestroy(DYN_CJThreadSpecificDataT* data) { /* TODO: ectype cleanup */ }

static void IterateFramesWithState(
    DYN_CJThreadSpecificDataT threadSpecificData, void (*callback)(DYN_VisitingStateT, void*), void* ctx
)
{
    DYN_VisitingStateT state = nullptr; // TODO implement 
    callback(state, ctx);
}

static void VisitGCFrameRoots(DYN_FrameDescT frame_desc, DYN_RootVisitorT root_visitor)
{
    auto fuh    = reinterpret_cast<Interpretation::DynamicFunctionHandle*>((uint8_t*) frame_desc.fp - FUH_SLOT_OFFSET);
    auto reader = reinterpret_cast<Decoder::ByteReader*>((uint8_t*) frame_desc.fp - READER_SLOT_OFFSET);
    auto curPos = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(reader->Cursor()) - reinterpret_cast<uintptr_t>(reader->Start())
    ); 
    
    auto bc = NOTNULL(fuh->bytecode.load(std::memory_order_acquire));

    const Interpretation::ReferenceInfo* refInfo = nullptr;
    for (auto& info : bc->referenceInfos) {
        if (info.rewrittenPos == curPos) {
            refInfo = &info;
            break;
        }
    }

    auto slotsStartAddr = ((uint8_t*) frame_desc.fp) - (READER_SLOT_OFFSET + bc->frameSize);
    for (auto& slotOffset : NOTNULL(refInfo)->refSlotOffsets) {
        auto slotAddr = slotsStartAddr + slotOffset;
        g_CJNativeInterfaceInstance.visitRootFromInterpreter(root_visitor, slotAddr);
    }
}

static void VisitFrameRootsMarking(
    DYN_VisitingStateT state, 
    DYN_FrameDescT frame_desc, 
    DYN_RootVisitorT root_visitor
)
{
    // TODO scan saved regs
    VisitGCFrameRoots(frame_desc, root_visitor);
}

static void VisitFrameRootsAdjusting(
    DYN_VisitingStateT state,
    DYN_FrameDescT frame_desc,
    DYN_RootVisitorT root_visitor,
    DYN_DerivedPtrVisitorT derived_ptr_visitor
)
{
    // scan saved regs
    VisitGCFrameRoots(frame_desc, root_visitor);
}

static void VisitFrameRootsExpansion(
    DYN_VisitingStateT state,
    DYN_FrameDescT frameDesc,
    DYN_RootVisitorT stackPtrVisitor,
    DYN_DerivedPtrVisitorT derivedPtrVisitor
)
{
    /* no-op */
}

static void VisitGlobalRoots(DYN_RootVisitorT visitor)
{
    auto& engine = Engine::GetEngineInstance();
    Engine::StaticsManager::Of(engine).VisitRefLocations([visitor](Engine::RefLocation* refLocation) {
        g_CJNativeInterfaceInstance.visitRootFromInterpreter(visitor, refLocation);
    });
}

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT void interpreter_bridge_init(
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    char const** options
);

CBC_EXPORT void engine_set_cbcpath(char const* cbcPath) { g_cbcPath = cbcPath; }

CBC_EXPORT void engine_set_main_cbc(char const* mainCbc) { g_mainCbc = mainCbc; }

CBC_EXPORT void engine_initialize() { EnsureEngineInitialized(); }

CBC_EXPORT void engine_enable_dasm() { Interpretation::Log::preparation.SetLogLevel(Logging::Level::TRACE); }

CBC_EXPORT void engine_enable_raw_dasm()
{
    engine_enable_dasm();
    Cbc::EnableRawDisasm();
}

CBC_EXPORT void* engine_get_entrypoint_trampoline(void)
{
    ASSERTION(g_Initialized, "Engine is not initialized");

    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto main = engine.FindMain(session, g_mainCbc);
    if (!main.has_value()) {
        return nullptr;
    }
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = fuhManager.AcquireTagged(session, main.value());
    return fuhManager.GetFunctionPtrForDirectCall(fuh);
}

CBC_EXPORT void interpreter_bridge_init(
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    char const** options
)
{
    // Order matters
    InitEnvOpts();
    Engine::g_table.ParseAndSet(size, options);

    g_CJNativeInterfaceInstance            = *rtInterf;
    interpInterf->version                  = 1;
    interpInterf->cjThreadSpecificDataSize = sizeof(Interpretation::Ectype);
    interpInterf->iteratorSize             = 0; // FIXME: remove
    interpInterf->c2iStubStartAddr         = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_start);
    interpInterf->c2iStubEndAddr           = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_end);
    interpInterf->cjThreadStart            = &FiberStart;
    interpInterf->cjThreadDestroy          = &FiberDestroy;

    interpInterf->iterateFramesWithState   = &IterateFramesWithState;
    interpInterf->visitFrameRootsExpansion = &VisitFrameRootsExpansion;
    interpInterf->visitFrameRootsMarking   = &VisitFrameRootsMarking;
    interpInterf->visitFrameRootsAdjusting = &VisitFrameRootsAdjusting;
    interpInterf->visitGlobalRoots         = &VisitGlobalRoots;

    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
    RTSupport::Initialize(&g_CJNativeInterfaceInstance);
}

} // extern "C"
