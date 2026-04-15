#include "resolver_impl.h"
#include "engine/symlevel/aot_table.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/region_data.h"
#include "method_impl.h"

namespace API {
namespace Impl {

//////////////////////////////////
// Resolver

Type* ResolverImpl::Resolve(Symlevel::Index<Type> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

Term* ResolverImpl::Resolve(Symlevel::Index<Symlevel::Term> index)
{
    ASSERTION(false, "not implemented yet");
    return nullptr;
}

Method* ResolverImpl::Resolve(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& cbcFile = session.CbcFileOf(method.GetFileId());

    auto methodRefOpt = cbcFile.GetRegionData().queryMethod(session, index);
    if (!methodRefOpt.has_value()) {
        // TODO: handle this case
        throw std::runtime_error("cannot resolve method ref");
    }

    auto methodRef = methodRefOpt.value();

    auto refTypeId = methodRef.RefType().GetIdentifier();
    switch (refTypeId.GetKind()) {
        case Symlevel::TemplateKind::TYPE: {
            Engine::Identifier<Symlevel::TypeDefinition> typeId(refTypeId.GetNum());
            auto refTypeDef = Symlevel::TypeDefinition::Resolve(session, typeId);

            auto candidates = refTypeDef.GetMethodIndex().FindMethods(session, methodRef.Name());

            ASSERTION(candidates.size() == 1, "not implemented yet");
            auto target = candidates[0];

            return session.Allocator().New<DirectMethodCbc>(session, target);
        }

        case Symlevel::TemplateKind::AOT_TYPE: {
            auto data = cbcFile.GetDirectCallAotTable().GetData(session, index);
            if (!data.has_value()) {
                // TODO: handle this case
                throw std::runtime_error("aot method ref has no aot data");
            }

            return session.Allocator().New<DirectMethodAot>(session, methodRef, data.value());
        }

        default: {
            ASSERTION(false, "should not reach here");
            return nullptr;
        }
    }
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

} // namespace Impl
} // namespace API
