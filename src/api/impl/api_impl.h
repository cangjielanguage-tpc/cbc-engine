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

    Type* Resolve(Symlevel::Index<Type> index) override;

    Term* Resolve(Symlevel::Index<Term> index) override;

    Method* Resolve(Symlevel::Index<Symlevel::MethodReference> index) override;

    InstanceField* Resolve(Symlevel::Index<InstanceField> index) override;

    StaticField* Resolve(Symlevel::Index<StaticField> index) override;

    std::optional<Type*> Resolve(Term* term) override;

    std::optional<Type*> TypeOf(Term* term) override;

    ~ResolverImpl() override;

private:
    Engine::Session& session;
    Engine::Identifier<Symlevel::MethodDefinition> method;
};

class MethodImpl final : public Method {
public:
    MethodImpl(Engine::Session& session, Symlevel::MethodDefinition def) : session(session), def(def) {}

    Term* ABISignature() override;

    std::optional<Type*> RefType() override;

    std::optional<Interpretation::FunctionHandle*> FUH() override;

    MethodFlags Flags() override;

    std::string_view FullName() override;

private:
    Engine::Session& session;
    Symlevel::MethodDefinition def;
};

} // namespace Impl
} // namespace API
