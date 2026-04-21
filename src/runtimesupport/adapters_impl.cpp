#include "asm_trampolines.h"
#include "interpreter/code.h"
#include "runtimesupport/adapters.h"

namespace RTSupport {

void* Adapters::GenericI2CCallInstance() { return reinterpret_cast<void*>(&Asm::engine_i2c_call); }

void* Adapters::I2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_i2i_call); }

void* Adapters::IregOnlyC2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_iregs_only_c2i_call); }

std::mutex g_directCallFuhsMutex;
static size_t directCallFuhsCount;

extern "C" Interpretation::ExecBytecodeInfo* engine_prepare_bytecode(Interpretation::DynamicFunctionHandle* fuh)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Prepare(session, fuh);
}

static void* GetAddressOfDirectCallTrampoline(int i)
{
    auto start = reinterpret_cast<char*>(&Asm::engine_trampolines_direct_start);
    return start + i * DIRECT_CALL_TRAMPOLINE_SIZE;
}

void* Adapters::GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh)
{
    std::lock_guard guard(g_directCallFuhsMutex);
    int i = 0;
    for (; i < directCallFuhsCount; i++) {
        if (Asm::engine_universal_direct_function_handles[i] == fuh) {
            return GetAddressOfDirectCallTrampoline(i);
        }
    }
    if (i == TRAMPOLINE_COUNT) {
        throw std::runtime_error("Too many direct calls");
    }
    directCallFuhsCount++;

    Asm::engine_universal_direct_function_handles[i] = fuh;
    return GetAddressOfDirectCallTrampoline(i);
}

} // namespace RTSupport
