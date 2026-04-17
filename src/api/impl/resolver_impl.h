#pragma once

#include "api/method.h"
#include "api/resolver.h"
#include "engine/engine.h"
#include "engine/symlevel/definitions.h"

namespace API {
namespace Impl {

class ResolverImpl final : public Resolver {
public:
    ResolverImpl(Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method)
        : Resolver(),
          session(session),
          method(method)
    {}

    Type* Resolve(Symlevel::Index<Symlevel::Terms::Term> index) override;

    DirectMethod* ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index) override;

    VirtualMethod* ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index) override;

    Field* Resolve(Symlevel::Index<Symlevel::FieldReference> index) override;

    std::optional<Type*> TypeOf(Symlevel::Terms::Term* term) override;

    ~ResolverImpl() override;

private:
    Engine::Session& session;
    Engine::Identifier<Symlevel::MethodDefinition> method;

    Type* Resolve(Symlevel::Terms::Term index);
};

} // namespace Impl
} // namespace API
