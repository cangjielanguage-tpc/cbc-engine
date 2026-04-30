#include "resolver_impl.h"
#include "engine/symlevel/references.h"
#include "field_impl.h"
#include "utils/assertion.h"

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Engine::Term> index)
{
    return nullptr;
}

Type* ResolverImpl::Resolve(Engine::Term term)
{
    return nullptr;
}

VirtualMethod* ResolverImpl::ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    return nullptr;
}

DirectMethod* ResolverImpl::ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index)
{
    return nullptr;
}

template <typename T> T* ResolverImpl::ResolveField(Symlevel::Index<Symlevel::FieldReference> index)
{
    return nullptr;
}

InstanceField* ResolverImpl::ResolveInstanceField(Symlevel::Index<Symlevel::FieldReference> index)
{
    return ResolveField<InstanceFieldImpl>(index);
}

StaticField* ResolverImpl::ResolveStaticField(Symlevel::Index<Symlevel::FieldReference> index)
{
    return ResolveField<StaticFieldImpl>(index);
}

std::optional<Type*> ResolverImpl::TypeOf(Engine::Term* term)
{
    FATAL("not implemented yet");
    return nullptr;
}

ResolverImpl::~ResolverImpl() = default;

} // namespace Impl
} // namespace API
