#pragma once

#include "interpreter/function_handle.h"
#include <cstdint>

// Runtime specific layer adapter.
// TODO: make it more generic for users.
namespace RTSupport {
struct Adapters {
    static void* GenericI2CCallInstance();
    static void* I2ICallInstance();
    static void* GenericC2ICallInstance();
    static void* IregOnlyC2ICallInstance();
    static void* GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh);
    static void* GetDynCallTrampoline(int fuhIdx);
    static void* GetDynCallTrampoline(int fuhIdx, bool sret);
    static void* C2ICall(uint32_t intArgCount, uint32_t floatArgCount);
};
} // namespace RTSupport
