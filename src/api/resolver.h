#pragma once

#include <optional>

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/index.h"
#include "field.h"
#include "method.h"
#include "term.h"
#include "type.h"

namespace Symlevel {

class MethodReference;
class FieldReference;

} // namespace Symlevel

namespace API {

class Resolver {
public:
    static std::unique_ptr<Resolver> Create(
        Engine::Session& session, Engine::Identifier<Symlevel::MethodDefinition> method
    );

    virtual Type* Resolve(Symlevel::Index<Type> index) = 0;

    virtual Term* Resolve(Symlevel::Index<Term> index) = 0;

    virtual Method* Resolve(Symlevel::Index<Symlevel::MethodReference> index) = 0;

    virtual InstanceField* Resolve(Symlevel::Index<InstanceField> index) = 0;

    virtual StaticField* Resolve(Symlevel::Index<StaticField> index) = 0;

    virtual std::optional<Type*> Resolve(Term* term) = 0;

    virtual std::optional<Type*> TypeOf(Term* term) = 0;

    virtual ~Resolver() = default;
};

} // namespace API
