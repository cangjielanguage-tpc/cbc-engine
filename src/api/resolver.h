#pragma once

#include <optional>

#include "engine/engine.h"
#include "engine/identifiers.h"
#include "engine/symlevel/index.h"
#include "engine/symlevel/terms.h"
#include "field.h"
#include "method.h"
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

    virtual Type* Resolve(Symlevel::Index<Symlevel::Terms::Term> index) = 0;

    virtual DirectMethod* ResolveDirectMethod(Symlevel::Index<Symlevel::MethodReference> index) = 0;

    virtual VirtualMethod* ResolveVirtualMethod(Symlevel::Index<Symlevel::MethodReference> index) = 0;

    virtual InstanceField* ResolveInstanceField(Symlevel::Index<Symlevel::FieldReference> index) = 0;

    virtual StaticField* ResolveStaticField(Symlevel::Index<Symlevel::FieldReference> index) = 0;

    virtual std::optional<Type*> TypeOf(Symlevel::Terms::Term* term) = 0;

    virtual ~Resolver() = default;
};

} // namespace API
