#include <mutex>

#include "adapters.h"
#include "asm_export.h"

namespace Interpretation {

std::mutex g_directCallFuhsMutex;
static size_t directCallFuhsCount;

extern "C" {

typedef char DirectTrampolineText[DIRECT_CALL_TRAMPOLINE_SIZE];

extern DirectTrampolineText engine_trampolines_direct_start[];
DynamicFunctionHandle* engine_universal_direct_function_handles[TRAMPOLINE_COUNT];
}

static I2Call g_overridenI2Call;

// TODO: Never inline
static ExecBytecodeInfo* PrepareBytecode(DynamicFunctionHandle* fuh)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto& manager = FunctionHandleManager::Of(session);
    return manager.Prepare(session, fuh);
}

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(g_directCallFuhsMutex);
    int i = 0;
    for (; i < directCallFuhsCount; i++) {
        if (engine_universal_direct_function_handles[i] == fuh) {
            return &engine_trampolines_direct_start[i];
        }
    }
    if (i == TRAMPOLINE_COUNT) {
        throw std::runtime_error("Too many direct calls");
    }
    engine_universal_direct_function_handles[i] = fuh;
    directCallFuhsCount                         = i + 1;
    return &engine_trampolines_direct_start[i];
}

I2Call PrepareI2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    if (g_overridenI2Call) {
        return g_overridenI2Call;
    }
    return nullptr;
}

C2Call PrepareC2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    return nullptr;
}

void SetI2CallForInterpreter(I2Call i2call)
{
#if defined(UNIT_TEST_MODE)
    g_overridenI2Call = i2call;
#endif
}

} // namespace Interpretation
