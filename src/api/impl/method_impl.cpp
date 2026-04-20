#include "method_impl.h"
#include "api/term.h"
#include "engine/symlevel/dependencies.h"
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

void* DirectMethodCbc::TargetAddr() { return nullptr; }

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
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> DirectMethodAot::FUH() { return std::nullopt; }

void* DirectMethodAot::TargetAddr()
{
    auto& deps       = session.CbcFileOf(ref.FileId()).GetDependencies();
    auto linkageName = aotData.GetLinkageName();

    return deps.FindTarget(linkageName);
}

MethodFlags DirectMethodAot::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

Symlevel::String DirectMethodAot::Name() { return ref.Name(); }

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

Symlevel::String VirtualMethodImpl::Name() { return ref.Name(); }

} // namespace Impl
} // namespace API
