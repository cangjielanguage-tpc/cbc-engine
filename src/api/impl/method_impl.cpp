#include "method_impl.h"
#include "api/term.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"

namespace API {
namespace Impl {

//////////////////////////////////
// DirectMethodCbc

Term* DirectMethodCbc::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodCbc::RefType()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodCbc::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

MethodFlags DirectMethodCbc::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodCbc::Name() { return Symlevel::Reader::Read(session, def.FileId(), def.NameOffset()); }

//////////////////////////////////
// DirectMethodAot

Term* DirectMethodAot::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodAot::RefType()
{
    auto refType = ref.RefType();
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodAot::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    ASSERTION(false, "not implemented yet");
    return std::nullopt;
}

MethodFlags DirectMethodAot::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodAot::Name() { return ref.Name(); }

} // namespace Impl
} // namespace API
