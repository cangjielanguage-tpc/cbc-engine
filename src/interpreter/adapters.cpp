#include <mutex>

#include "adapters.h"
#include "asm_export.h"

namespace Interpretation {

std::mutex g_directCallFuhsMutex;
static size_t directCallFuhsCount;

extern "C" {
extern char engine_trampolines_direct_start[];
extern char engine_trampolines_direct_end[];
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

static uint64_t FakeTrampoline0()
{
    DynamicFunctionHandle* fuh = engine_universal_direct_function_handles[0];
    auto bytecode              = fuh->bytecode.load();
    if (!bytecode) {
        bytecode = PrepareBytecode(fuh);
    }

    printf("Hello from trampoline 0\n");
    return 0;
}

static void* GetDirectCallTrampolineByIdx(int i)
{
    auto sectionSize    = engine_trampolines_direct_end - engine_trampolines_direct_start;
    auto trampolineSize = sectionSize / TRAMPOLINE_COUNT;
    return engine_trampolines_direct_start + trampolineSize * i;
}

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(g_directCallFuhsMutex);
    int i = 0;
    for (; i < directCallFuhsCount; i++) {
        if (engine_universal_direct_function_handles[i] == fuh) {
            return GetDirectCallTrampolineByIdx(i);
        }
    }
    if (i == TRAMPOLINE_COUNT) {
        throw std::runtime_error("Too many direct calls");
    }
    engine_universal_direct_function_handles[i] = fuh;
    directCallFuhsCount                         = i + 1;
    return GetDirectCallTrampolineByIdx(i);
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
