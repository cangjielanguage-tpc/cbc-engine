#include "api_impl.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/region_data.h"

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Type> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

Term* ResolverImpl::Resolve(Symlevel::Index<Symlevel::Terms::Term> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

Method* ResolverImpl::Resolve(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& regionData = session.CbcFileOf(method.GetFileId()).GetRegionData();

    auto methodRefOpt = regionData.queryMethod(session, index);
    ASSERTION(methodRefOpt.has_value(), "cannot resolve method ref");
    auto methodRef = methodRefOpt.value();

    Engine::Identifier<Symlevel::TypeDefinition> typeId(methodRef.RefType().GetIdentifier().GetNum());
    auto refTypeDef = Symlevel::TypeDefinition::Resolve(session, typeId);

    auto candidates = refTypeDef.GetMethodIndex().FindMethods(session, methodRef.Name());

    ASSERTION(candidates.size() == 1, "not implemented yet");
    auto target = candidates[0];

    return session.Allocator().New<MethodImpl>(session, target);
}

InstanceField* ResolverImpl::Resolve(Symlevel::Index<InstanceField> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

StaticField* ResolverImpl::Resolve(Symlevel::Index<StaticField> index)
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::Resolve(Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> ResolverImpl::TypeOf(Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

//////////////////////////////////
// Method

Term* MethodImpl::ABISignature()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Type*> MethodImpl::RefType()
{
    FATAL("not implemented yet");
    return nullptr;
}

std::optional<Interpretation::FunctionHandle*> MethodImpl::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

MethodFlags MethodImpl::Flags()
{
    FATAL("not implemented yet");
    return MethodFlags();
}

std::string_view MethodImpl::FullName()
{
    FATAL("not implemented yet");
    return "<empty>";
}

} // namespace Impl
} // namespace API
