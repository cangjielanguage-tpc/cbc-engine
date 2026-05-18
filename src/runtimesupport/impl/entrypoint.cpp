#include "runtimesupport/impl/entrypoint.h"

#include <filesystem>
#include <mutex>

#include "RTInterface.h"
#include "asm_export.h"
#include "asm_trampolines.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/options.h"
#include "engine/symlevel/io/filesystem.h"
#include "gc_support.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "runtimesupport/impl/rt_syms.h"
#include "utils/logger.h"
#include "utils/ostream.h"
#include "utils/rt_logger.h"

DYN_CJNativeInterfaceT g_CJNativeInterfaceInstance;

static std::mutex g_InitializationGuard;
static bool g_Initialized;
static bool g_OptionsInitialized;

static void InitEnvOpts()
{
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

static void FiberStart(DYN_CJThreadSpecificDataT* data) { *data = nullptr; }

extern "C" Interpretation::Ectype* FiberDataInit(DYN_CJThreadSpecificDataT* data) __asm__("engine_fiber_data_init");

Interpretation::Ectype* FiberDataInit(DYN_CJThreadSpecificDataT* data)
{
    ASSERTION(*data == nullptr, "Incorrect data value: %p", *data);

    auto ectype = new Interpretation::Ectype();
    if (!ectype) {
        FATAL("Ectype allocation error");
    }

    *data = ectype;

    RTSupport::Log::rt.Log(Logging::Level::INFO, [&data](Stream::Output& out) {
        out.PrintFmtLn("fiber init, fsd addr: %p, ectype addr: %p", data, *data);
    });

    return ectype;
}

static void FiberDestroy(DYN_CJThreadSpecificDataT* data)
{
    if (*data == nullptr) {
        // ectype wasn't initialized for this fiber, nothing to do here
        return;
    }

    RTSupport::Log::rt.Log(Logging::Level::INFO, [&data](Stream::Output& out) {
        out.PrintFmtLn("fiber destroy, fsd addr: %p, ectype addr: %p", data, *data);
    });
    delete static_cast<Interpretation::Ectype*>(*data)->Checked();
}

static void IterateFramesWithState(
    DYN_CJThreadSpecificDataT threadSpecificData, void (*callback)(DYN_VisitingStateT, void*), void* ctx
)
{
    GCSupport::IterateFramesWithState(threadSpecificData, callback, ctx);
}

static void VisitFrameRootsMarking(DYN_VisitingStateT state, DYN_FrameDescT frame_desc, DYN_RootVisitorT root_visitor)
{
    GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor);
}

static void VisitFrameRootsAdjusting(
    DYN_VisitingStateT state,
    DYN_FrameDescT frame_desc,
    DYN_RootVisitorT root_visitor,
    DYN_DerivedPtrVisitorT derived_ptr_visitor
)
{
    GCSupport::VisitGCFrameRoots(state, frame_desc, root_visitor);
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

static void VisitGlobalRoots(DYN_RootVisitorT visitor) { GCSupport::VisitGlobalRoots(visitor); }

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT int interpreter_bridge_init(
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    const char* const* options
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

CBC_EXPORT int interpreter_bridge_init(
    struct DYN_InterpreterInterfaceT* interpInterf,
    struct DYN_CJNativeInterfaceT* rtInterf,
    int size,
    const char* const* options
)
{
    static_assert(std::is_same_v<decltype(&interpreter_bridge_init), DYN_InitRt>);

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

    Asm::engine_carrier_specific_offset  = g_CJNativeInterfaceInstance.carrierSpecificOffset;
    Asm::engine_cjthread_specific_offset = g_CJNativeInterfaceInstance.cjThreadSpecificOffset;

    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
    RTSupport::Initialize(&g_CJNativeInterfaceInstance);

    return 0;
}

} // extern "C"
