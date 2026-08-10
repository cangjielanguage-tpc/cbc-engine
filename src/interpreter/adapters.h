#pragma once

#include "function_handle.h"
#include "resolution/resolution.h"

namespace Interpretation {

enum class CallAdapter : uint8_t {
    I2I,
    IREG_C2I,
    C2I,
    I2C,
    LAST
};

/// Acquire trampoline to the interpreter for given function handle.
void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh);

/// Returns appropriate I2Call adapter for given method.
I2Call PrepareI2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef);

/// Returns appropriate I2Call adapter for given method.
/// Expects that method is dynamic (cbc).
C2Call PrepareC2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef);

CallAdapter AdapterFor(Resolution::VirtualCall const& vc);

CallAdapter AdapterFor(Resolution::InterfaceCall const& vc);

void* AdapterOf(CallAdapter adapter);

} // namespace Interpretation
