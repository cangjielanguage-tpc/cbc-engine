#pragma once

#include "function_handle.h"
#include <cstdint>

namespace Resolution {
struct VirtualCall;
struct InterfaceCall;
} // namespace Resolution

namespace Interpretation {

enum class CallAdapter : uint8_t {
    I2I,
    IREG_ONLY_C2I,
    GENERIC_I2C,
    IREG_ONLY_I2C,
    LAST
};

struct RegRequirements {
    uint32_t intregs;
    uint32_t floatregs;
};

/// Acquire trampoline to the interpreter for given function handle.
void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh);

/// Returns appropriate I2Call adapter for given method.
I2Call PrepareI2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef);

/// Returns appropriate I2Call adapter for given method.
/// Expects that method is dynamic (cbc).
C2Call PrepareC2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef);

CallAdapter AdapterFor(Resolution::VirtualCall const& vc);

CallAdapter AdapterFor(Resolution::InterfaceCall const& ic);

void* AdapterFor(CallAdapter adapter);

} // namespace Interpretation
