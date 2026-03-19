#include <filesystem>

#include "cbc_engine.h"
#include "cjnative.h"
#include "engine/engine.h"
#include "engine/symlevel/io/filesystem.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "runtime_impl.h"

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

static void FiberStart(fiber_specific_data_t* data) { /* no-op */ }

static void FiberDestroy(fiber_specific_data_t* data) { /* TODO: ectype cleanup */ }

extern "C" {
/// This symbol is exported to the runtime, which would initialize engine.
CBC_EXPORT void interpreter_bridge_init(
    size_t size,
    char const** options,
    struct interpreter_interface_t* interpInterf,
    struct cjnative_interface_t* rtInterf
);

/// Internal engine symbols declared in ASM.
void engine_c2i_call_pc_start();
void engine_c2i_call_pc_end();

CBC_EXPORT void engine_set_cbcpath(char const* cbcPath) { g_cbcPath = cbcPath; }

CBC_EXPORT void engine_set_main_cbc(char const* mainCbc) { g_mainCbc = mainCbc; }

CBC_EXPORT void* engine_get_entrypoint_trampoline(void)
{
    EnsureEngineInitialized();
    auto& engine = Engine::GetEngineInstance();

    Engine::Session session(engine);

    auto main = engine.FindMain(session, g_mainCbc);
    if (!main.has_value()) {
        return nullptr;
    }
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = fuhManager.AcquireTagged(session, main.value());
    return fuhManager.GetFunctionPtr(fuh);
}

CBC_EXPORT void interpreter_bridge_init(
    size_t size,
    char const** options,
    struct interpreter_interface_t* interpInterf,
    struct cjnative_interface_t* rtInterf
)
{
    (void)size;
    (void)options;

    g_CJNativeInterfaceInstance             = *rtInterf;
    interpInterf->version                   = 1;
    interpInterf->fiber_specific_data_size  = sizeof(Interpretation::Ectype);
    interpInterf->iterator_size             = 0; // FIXME: remove
    interpInterf->c2iVirtualExecutorAddr    = reinterpret_cast<uintptr_t>(&engine_c2i_call_pc_start);
    interpInterf->c2iVirtualExecutorEndAddr = reinterpret_cast<uintptr_t>(&engine_c2i_call_pc_end);
    interpInterf->fiber_destroy             = &FiberDestroy;
    interpInterf->fiber_start               = &FiberStart;

    Interpretation::InitializeRuntimeInterface();
}

} // extern "C"
