#include "runtimesupport/adapters.h"
#include "asm_trampolines.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include "runtimesupport/impl/typeinfo_ext.h"
#include "utils/assertion.h"

namespace RTSupport {

void* Adapters::GenericC2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_all_regs_c2i_call); }

void* Adapters::IregOnlyC2ICallInstance() { return reinterpret_cast<void*>(&Asm::engine_iregs_only_c2i_call); }

void* Adapters::C2ICall(Interpretation::AbiInfo info)
{
    if (info.fregParamCount > 0) {
        return Adapters::GenericC2ICallInstance();
    }
    return Adapters::IregOnlyC2ICallInstance();
}

std::mutex g_directCallFuhsMutex;
static size_t directCallFuhsCount;

extern "C" Interpretation::DynamicFunctionHandle* engine_prepare_bytecode(
    Interpretation::DynamicFunctionHandle* fuh, Interpretation::ExecBytecodeInfo** bytecodeInfo
)
{
    Engine::Session session(Engine::GetEngineInstance());
    auto& manager       = Interpretation::FunctionHandleManager::Of(session);
    auto preparedBcInfo = manager.Prepare(session, fuh);
    *bytecodeInfo       = preparedBcInfo.info;
    if (preparedBcInfo.success) {
        // Usual path: rewriting went well and there is no need for patching or anything
        return nullptr;
    } else {
        return preparedBcInfo.fuh;
    }
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

// Tries to restore the DynamicFunctionHandle from the direct call trampoline address.
Interpretation::DynamicFunctionHandle* Adapters::TryGetDirectCallTargetFuh(void* trampoline)
{
    auto start   = reinterpret_cast<uintptr_t>(&Asm::engine_trampolines_direct_start);
    auto address = reinterpret_cast<uintptr_t>(trampoline);
    if (address < start) {
        return nullptr;
    }

    auto offset = address - start;
    if (offset >= TRAMPOLINE_COUNT * DIRECT_CALL_TRAMPOLINE_SIZE || offset % DIRECT_CALL_TRAMPOLINE_SIZE != 0) {
        return nullptr;
    }

    auto idx = offset / DIRECT_CALL_TRAMPOLINE_SIZE;
    std::lock_guard guard(g_directCallFuhsMutex);
    if (idx >= directCallFuhsCount) {
        return nullptr;
    }
    return static_cast<Interpretation::DynamicFunctionHandle*>(Asm::engine_universal_direct_function_handles[idx]);
}

void* Adapters::GetDynCallTrampoline(int fuhIdx, bool sret)
{
    auto start = sret ? reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_sret_start)
                      : reinterpret_cast<char*>(&Asm::engine_trampolines_dyn_start);
    return start + fuhIdx * DYN_CALL_TRAMPOLINE_SIZE;
}

} // namespace RTSupport
