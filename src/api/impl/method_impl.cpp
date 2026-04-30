#include "method_impl.h"
#include "engine/symlevel/dependencies.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/region_data.h"

namespace API {
namespace Impl {

using Term = Engine::Term;

//////////////////////////////////
// DirectMethodCbc

Term* DirectMethodCbc::ABISignature()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodCbc::RefType()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodCbc::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

void* DirectMethodCbc::TargetAddr() { return nullptr; }

MethodFlags DirectMethodCbc::Flags()
{
    FATAL("not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodCbc::Name() { return Symlevel::Reader::Read(session, def.FileId(), def.NameOffset()); }

//////////////////////////////////
// DirectMethodAot

Term* DirectMethodAot::ABISignature()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> DirectMethodAot::RefType()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodAot::FUH() { return std::nullopt; }

void* DirectMethodAot::TargetAddr()
{
    return nullptr;
}

MethodFlags DirectMethodAot::Flags()
{
    FATAL("not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodAot::Name() { return std::string_view(""); }

//////////////////////////////////
// VirtualMethodAot

Term* VirtualMethodImpl::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> VirtualMethodImpl::RefType()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> VirtualMethodImpl::FUH() { return std::nullopt; }

uint16_t VirtualMethodImpl::VNum() { return vnum; }

uint16_t VirtualMethodImpl::ExtDefNum() { return extDefNum; }

MethodFlags VirtualMethodImpl::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String VirtualMethodImpl::Name() { return std::string_view(""); }

} // namespace Impl
} // namespace API
