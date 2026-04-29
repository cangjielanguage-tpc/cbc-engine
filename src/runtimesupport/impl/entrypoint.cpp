#include <filesystem>

#include "RTInterface.h"
#include "asm_trampolines.h"
#include "cbc/isa_disasm.h"
#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/symlevel/io/filesystem.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"

static std::mutex g_InitializationGuard;
static bool g_Initialized;
static char const* g_cbcPath;
static char const* g_mainCbc;

/// Initialize engine from launcher.
static void EnsureEngineInitialized()
{
    std::lock_guard guard(g_InitializationGuard);
    if (g_Initialized) {
        return;
    }

    Engine::Loader loader;
    loader.Load(IO::OpenFile(std::filesystem::path(g_mainCbc)), g_mainCbc);
    loader.Build();
    g_Initialized = true;
}

static void FiberStart(DYN_CJThreadSpecificDataT* data) { /* no-op */ }

static void FiberDestroy(DYN_CJThreadSpecificDataT* data) { /* TODO: ectype cleanup */ }

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

CBC_EXPORT void engine_enable_dasm() { Cbc::EnableDisasm(); }

CBC_EXPORT void engine_enable_raw_dasm() { Cbc::EnableRawDisasm(); }

CBC_EXPORT void* engine_get_entrypoint_trampoline(void)
{
    ASSERTION(g_Initialized, "Engine is not initialized");

    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto main = engine.FindMethod(session, g_mainCbc, "default", "main");
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
    (void)size;
    (void)options;

    g_CJNativeInterfaceInstance            = *rtInterf;
    interpInterf->version                  = 1;
    interpInterf->cjThreadSpecificDataSize = sizeof(Interpretation::Ectype);
    interpInterf->iteratorSize             = 0; // FIXME: remove
    interpInterf->c2iStubStartAddr         = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_start);
    interpInterf->c2iStubEndAddr           = reinterpret_cast<uintptr_t>(&Asm::engine_c2i_call_pc_end);
    interpInterf->cjThreadStart            = &FiberStart;
    interpInterf->cjThreadDestroy          = &FiberDestroy;

    Asm::engine_newobject_function = g_CJNativeInterfaceInstance.objectAlloc;
}

} // extern "C"
