#include "adapters.h"
#include "engine/image/reader.h"
#include "engine/image/type_kind.h"
#include "engine/terms.h"
#include "function_handle.h"
#include "resolution/resolution.h"

#include "runtimesupport/adapters.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Interpretation {

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh) { return RTSupport::Adapters::GetDirectCallTrampoline(fuh); }

RegRequirements CountRegs(Engine::Term& signature)
{
    ASSERT(signature.GetKind() == Engine::TermKind::FUNCTIONAL);
    size_t size       = signature.GetLength();
    uint32_t integers = 0;
    uint32_t floats   = 0;

    for (size_t i = 0; i < size; i++) {
        auto subterm = signature.Subterm(i);
        if (subterm.IsFloat()) {
            floats++;
        } else {
            integers++;
        }
    }
    return RegRequirements { integers, floats };
}

I2Call PrepareI2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    return reinterpret_cast<I2Call>(RTSupport::Adapters::I2ICallInstance());
}

C2Call PrepareC2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    auto def          = Decode::Read(session, methodDef);
    auto signature    = Engine::TermManager::Resolve(session, def.Signature());
    auto requirements = CountRegs(signature);
    return RTSupport::Adapters::C2ICall(requirements);
}

constexpr size_t ADAPTERS_AMOUNT = static_cast<size_t>(CallAdapter::LAST);

std::array<void*, ADAPTERS_AMOUNT> adapters = {
    RTSupport::Adapters::I2ICallInstance(),
    RTSupport::Adapters::IregOnlyC2ICallInstance(),
    RTSupport::Adapters::GenericI2CCallInstance(),
    RTSupport::Adapters::IregOnlyI2CCallInstance(),
};

static CallAdapter ChooseDynamicAdapter(RegRequirements reqs)
{
    // if (reqs.floatregs == 0) {
    //     return CallAdapter::IREG_ONLY_I2C;
    // } else {
    return CallAdapter::GENERIC_I2C;
    // }
}

CallAdapter AdapterFor(Resolution::VirtualCall const& vc)
{
    auto& signature = vc->signature.term;
    auto reqs       = CountRegs(signature);
    return ChooseDynamicAdapter(reqs);
}

CallAdapter AdapterFor(Resolution::InterfaceCall const& ic)
{
    auto& signature = ic->signature.term;
    auto reqs       = CountRegs(signature);
    return ChooseDynamicAdapter(reqs);
}

void* AdapterFor(CallAdapter adapter) { return adapters[static_cast<size_t>(adapter)]; }

} // namespace Interpretation
