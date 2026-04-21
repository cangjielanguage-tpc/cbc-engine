#pragma once

#include "interpreter/function_handle.h"

// Runtime specific layer adapter.
// TODO: make it more generic for users.
namespace RTSupport {
struct Adapters {
    static void* GenericI2CCallInstance();
    static void* I2ICallInstance();
    static void* IregOnlyC2ICallInstance();
    static void* GetDirectCallTrampoline(Interpretation::DynamicFunctionHandle* fuh);
};
} // namespace RTSupport
