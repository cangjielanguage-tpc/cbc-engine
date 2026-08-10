#include "adapters.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/type_kind.h"
#include "engine/terms.h"
#include "function_handle.h"

#include "resolution/resolution.h"
#include "runtimesupport/adapters.h"
#include <cstdint>

namespace Interpretation {

using CallIdx = uint8_t;

static constexpr size_t ADAPTERS_AMOUNT = static_cast<size_t>(CallAdapter::LAST);

static std::array<void*, ADAPTERS_AMOUNT> adapters = {
    RTSupport::Adapters::I2ICallInstance(),
    RTSupport::Adapters::IregOnlyC2ICallInstance(),
    RTSupport::Adapters::GenericC2ICallInstance(),
    RTSupport::Adapters::GenericI2CCallInstance(),
};

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh) { return RTSupport::Adapters::GetDirectCallTrampoline(fuh); }

struct RegRequirements {
    uint32_t intregs;
    uint32_t floatregs;
};

static RegRequirements CountRegs(Engine::Term& signature)
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
    return RegRequirements { .intregs = integers, .floatregs = floats };
}

CallAdapter AdapterFor(Resolution::VirtualCall const& vc)
{
    //
    return CallAdapter::I2C;
}

CallAdapter AdapterFor(Resolution::InterfaceCall const& ic)
{
    //
    return CallAdapter::I2C;
}

void* AdapterOf(CallAdapter adapter) { return adapters[static_cast<size_t>(adapter)]; }

static C2Call ChooseC2Call(Engine::Term& sig)
{
    RegRequirements reqs = CountRegs(sig);
    return RTSupport::Adapters::C2ICall(reqs.intregs, reqs.floatregs);
}

static I2Call ChooseI2Call(Engine::Term& _sig)
{
    return reinterpret_cast<I2Call>(RTSupport::Adapters::I2ICallInstance());
}

I2Call PrepareI2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    auto def       = Symlevel::MethodDefinition::Resolve(session, methodDef);
    auto signature = Engine::TermManager::Resolve(session, def.Signature());
    return ChooseI2Call(signature);
}

C2Call PrepareC2Call(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> methodDef)
{
    auto def       = Symlevel::MethodDefinition::Resolve(session, methodDef);
    auto signature = Engine::TermManager::Resolve(session, def.Signature());
    return ChooseC2Call(signature);
}

} // namespace Interpretation
