#pragma once

#include "interpreter/code.h"
#include "interpreter/function_handle.h"
#include <cstdint>

// Runtime specific layer adapter.
// TODO: make it more generic for users.
namespace RTSupport {
struct Adapters {
    static void* GenericC2ICallInstance();
    static void* IregOnlyC2ICallInstance();
    static void* GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh);
    static Interpretation::DynamicFunctionHandle* TryGetDirectCallTarget(void* entry);
    static void* GetDynCallTrampoline(int fuhIdx, bool sret);
    static void* C2ICall(Interpretation::AbiInfo info);
};
} // namespace RTSupport
