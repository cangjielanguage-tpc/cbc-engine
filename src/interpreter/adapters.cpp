#include "adapters.h"
#include "engine/decode/reader.h"
#include "engine/terms.h"
#include "function_handle.h"

#include "interpreter/code.h"
#include "runtimesupport/adapters.h"

namespace Interpretation {

void* GetDirectCallTrampoline(DynamicFunctionHandle* fuh) { return RTSupport::Adapters::GetDirectCallTrampoline(fuh); }

C2Call PrepareC2Call(Engine::Session& session, Image::Identifier<Image::MethodDefinition> methodDef)
{
    auto def       = Decode::Read(session, methodDef);
    auto flags     = def.GetABIFlags();
    auto signature = Engine::TermManager::Resolve(session, def.Signature());
    auto abiInfo = Interpretation::BuildAbiInfo(
        session,
        Engine::TermManager::Resolve(session, def.Signature()),
        {
            .isSRet     = flags.Is(Image::MethodRefFlag::SRET),
            .isMut      = def->abiKind == Image::MethodAbiKind::MUT,
            .hasOuterTi = flags.Is(Image::MethodRefFlag::HAS_OUTER_TI),
            .abiKind    = def->abiKind,
            .funcVars   = def->arity,
        }
    );
    return RTSupport::Adapters::C2ICall(abiInfo);
}

} // namespace Interpretation
