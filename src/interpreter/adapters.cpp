#include <mutex>

#include "adapters.h"
#include "asm_export.h"
#include "code.h"
#include "function_handle.h"

#include "asm_trampolines.h"

namespace Interpretation {

std::mutex g_directCallFuhsMutex;
static size_t directCallFuhsCount;
static I2Call g_overridenI2Call;

extern "C" ExecBytecodeInfo* engine_prepare_bytecode(DynamicFunctionHandle* fuh)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto& manager = FunctionHandleManager::Of(session);
    return manager.Prepare(session, fuh);
}

static void* GetDirectCallTrampoline(int i)
{
    auto start = reinterpret_cast<char*>(&Asm::engine_trampolines_direct_start);
    return start + i * DIRECT_CALL_TRAMPOLINE_SIZE;
}

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(g_directCallFuhsMutex);
    int i = 0;
    for (; i < directCallFuhsCount; i++) {
        if (Asm::engine_universal_direct_function_handles[i] == fuh) {
            return GetDirectCallTrampoline(i);
        }
    }
    if (i == TRAMPOLINE_COUNT) {
        throw std::runtime_error("Too many direct calls");
    }
    directCallFuhsCount++;

    Asm::engine_universal_direct_function_handles[i] = fuh;
    return GetDirectCallTrampoline(i);
}

I2Call PrepareI2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    if (g_overridenI2Call) {
        return g_overridenI2Call;
    }
    return reinterpret_cast<void*>(&Asm::engine_i2i_call);
}

C2Call PrepareC2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    return reinterpret_cast<C2Call>(&Asm::engine_iregs_only_c2i_call);
}

void SetI2CallForInterpreter(I2Call i2call) { g_overridenI2Call = i2call; }

} // namespace Interpretation
