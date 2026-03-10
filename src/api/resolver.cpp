#include "resolver.h"
#include "impl/api_impl.h"

namespace API {

std::unique_ptr<Resolver> Resolver::Create(
    Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method
)
{
    return std::make_unique<Impl::ResolverImpl>(session, method);
}

} // namespace API
