#include "api_impl.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/region_data.h"

namespace API {
namespace Impl {

// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Type> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

Term* ResolverImpl::Resolve(Symlevel::Index<Term> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

Method* ResolverImpl::Resolve(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& regionData = session.CbcFileOf(method.GetFileId()).GetRegionData();

    auto methodRef = regionData.queryMethod(session, index);

    // TODO: use term for resolving
    // TODO: class name
    auto refTypeOpt = session.GetEngine().FindType(session, std::string_view("default"));
    ASSERT(refTypeOpt);

    auto& refType = *refTypeOpt;
    auto defs     = refType.GetMethodIndex().FindMethods(session, methodRef.Name());
    ASSERT(defs.size() == 1);

    auto def = defs[0];
    // return new MethodImpl(session, def);
    return nullptr; // FIXME: full implementation is needed to generate MethodImpl vtable
}

InstanceField* ResolverImpl::Resolve(Symlevel::Index<InstanceField> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

StaticField* ResolverImpl::Resolve(Symlevel::Index<StaticField> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::Resolve(Term* term)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::TypeOf(Term* term)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

// Method

Term* MethodImpl::ABISignature()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Type*> MethodImpl::RefType()
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> MethodImpl::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

MethodFlags MethodImpl::Flags()
{
    ASSERTION(false, "not implemented yet");
    return MethodFlags();
}

std::string_view MethodImpl::FullName()
{
    ASSERTION(false, "not implemented yet");
    return "<empty>";
}

} // namespace Impl
} // namespace API
