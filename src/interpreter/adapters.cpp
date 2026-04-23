#include "adapters.h"
#include "function_handle.h"

#include "runtimesupport/adapters.h"

namespace Interpretation {

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh) { return RTSupport::Adapters::GetDirectCallTrampoline(fuh); }

I2Call PrepareI2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    return reinterpret_cast<I2Call>(RTSupport::Adapters::I2ICallInstance());
}

C2Call PrepareC2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    return reinterpret_cast<C2Call>(RTSupport::Adapters::IregOnlyC2ICallInstance());
}

} // namespace Interpretation
