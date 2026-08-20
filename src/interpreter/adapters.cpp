#include "adapters.h"
#include "engine/image/reader.h"
#include "engine/image/type_kind.h"
#include "engine/terms.h"
#include "function_handle.h"

#include "runtimesupport/adapters.h"

namespace Interpretation {

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh) { return RTSupport::Adapters::GetDirectCallTrampoline(fuh); }

void* CountRegs(Engine::Term& signature)
{
    ASSERT(signature.GetKind() == Engine::TermKind::FUNCTIONAL);
    size_t size     = signature.GetLength();
    size_t integers = 0;
    size_t floats   = 0;

    for (size_t i = 0; i < size; i++) {
        auto subterm = signature.Subterm(i);
        if (subterm.IsFloat()) {
            floats++;
        } else {
            integers++;
        }
    }
    return RTSupport::Adapters::C2ICall(integers, floats);
}

I2Call PrepareI2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    return reinterpret_cast<I2Call>(RTSupport::Adapters::I2ICallInstance());
}

C2Call PrepareC2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    auto def       = Image::Reader::Read(session, methodDef);
    auto signature = Engine::TermManager::Resolve(session, def.Signature());
    return reinterpret_cast<C2Call>(CountRegs(signature));
}

} // namespace Interpretation
