#include "runtimesupport/adapters.h"
#include "asm_trampolines.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"

namespace RTSupport {

void* Adapters::GenericI2CCallInstance() { return reinterpret_cast<void*>(&Asm::engine_i2c_call); }

void* Adapters::I2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_i2i_call); }

void* Adapters::GenericC2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_all_regs_c2i_call); }

void* Adapters::IregOnlyC2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_iregs_only_c2i_call); }

void* Adapters::C2ICall(uint32_t intArgCount, uint32_t floatArgCount)
{
    if (floatArgCount == 0) {
        return Adapters::IregOnlyC2ICallInstance();
    } else {
        return Adapters::GenericC2ICallInstance();
    }
}

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
        FATAL("Too many direct call links");
    }
    directCallFuhsCount++;

    Asm::engine_universal_direct_function_handles[i] = fuh;
    return GetAddressOfDirectCallTrampoline(i);
}

void* Adapters::GetDynCallTrampoline(int fuhIdx) { return GetDynCallTrampoline(fuhIdx, false); }

void* Adapters::GetDynCallTrampoline(int fuhIdx, bool sret)
{
    auto start = sret ? reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_sret_start)
                      : reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_start);
    return start + fuhIdx * DYN_CALL_TRAMPOLINE_SIZE;
}

} // namespace RTSupport
