#include "resolver.h"
#include "impl/api_impl.h"

namespace API {

Resolver* Resolver::Create(Engine::Session& session, IO::FileId fileId)
{
    return new Impl::ResolverImpl(session, fileId);
}

} // namespace API
