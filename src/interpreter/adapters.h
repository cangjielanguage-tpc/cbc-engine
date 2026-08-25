#pragma once

#include "function_handle.h"

namespace Interpretation {

/// Acquire trampoline to the interpreter for given function handle.
void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh);

/// Returns appropriate I2Call adapter for given method.
I2Call PrepareI2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef);

/// Returns appropriate I2Call adapter for given method.
/// Expects that method is dynamic (cbc).
C2Call PrepareC2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef);

} // namespace Interpretation
