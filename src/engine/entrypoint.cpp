#include "cbc_engine.h"
#include "engine/engine.h"
#include "interpreter/function_handle.h"

extern "C" {

static char const* g_cbcPath;
static char const* g_mainCbc;

CBC_EXPORT void* engine_get_entrypoint_trampoline(void)
{
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

CBC_EXPORT void engine_set_cbcpath(char const* cbcPath) { g_cbcPath = cbcPath; }

CBC_EXPORT void engine_set_main_cbc(char const* mainCbc) { g_mainCbc = mainCbc; }

} // extern "C"
