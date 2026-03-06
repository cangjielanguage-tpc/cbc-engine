#include "adapters.h"

namespace Interpretation {

constexpr size_t MAX_DIRECT_CALL_TRAMPOLINES_COUNT = 2048;
static DynamicFunctionHandle* directCallFuhs[MAX_DIRECT_CALL_TRAMPOLINES_COUNT];

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
    DynamicFunctionHandle* fuh = directCallFuhs[0];
    auto bytecode              = fuh->bytecode.load();
    if (!bytecode) {
        bytecode = PrepareBytecode(fuh);
    }

    printf("Hello from trampoline 0\n");
    return 0;
}

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh)
{
    directCallFuhs[0] = fuh;
    return (void*)&FakeTrampoline0;
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
