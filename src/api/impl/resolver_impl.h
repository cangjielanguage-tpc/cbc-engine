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

    Type* Resolve(Symlevel::Index<Engine::Term> index) override;

    DirectMethod* ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index) override;

    VirtualMethod* ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index) override;

    InstanceField* ResolveInstanceField(Symlevel::Index<Symlevel::FieldReference> index) override;

    StaticField* ResolveStaticField(Symlevel::Index<Symlevel::FieldReference> index) override;

    std::optional<Type*> TypeOf(Engine::Term* term) override;

    ~ResolverImpl() override;

private:
    Engine::Session& session;
    Engine::Identifier<Symlevel::MethodDefinition> method;

    Type* Resolve(Engine::Term index);

    template <typename T> T* ResolveField(Symlevel::Index<Symlevel::FieldReference> index);
};

} // namespace Impl
} // namespace API
