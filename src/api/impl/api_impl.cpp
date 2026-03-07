#include "api_impl.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/region_data.h"

namespace API {
namespace Impl {

Method* ResolverImpl::Resolve(Symlevel::Index<Symlevel::MethodReference> index)
{
    auto& regionData = session.CbcFileOf(fileId).GetRegionData();

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

std::optional<Interpretation::FunctionHandle*> MethodImpl::FUH()
{
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    return manager.Acquire(session, def.GetIdentifier());
}

} // namespace Impl
} // namespace API
