#pragma once

#include "engine/engine.h"
#include "engine/symlevel/index.h"
#include "field.h"
#include "method.h"
#include "term.h"
#include "type.h"
#include <optional>

namespace Symlevel {

class MethodReference;
class FieldReference;

} // namespace Symlevel

namespace API {

class Resolver {
public:
    static Resolver* Create(Engine::Session& session, IO::FileId fileId);

    virtual Type* Resolve(Symlevel::Index<Type> index) = 0;

    virtual Term* Resolve(Symlevel::Index<Term> index) = 0;

    virtual Method* Resolve(Symlevel::Index<Symlevel::MethodReference> index) = 0;

    virtual InstanceField* Resolve(Symlevel::Index<InstanceField> index) = 0;

    virtual StaticField* Resolve(Symlevel::Index<StaticField> index) = 0;

    virtual std::optional<Type*> Resolve(Term* term) = 0;

    virtual std::optional<Type*> TypeOf(Term* term) = 0;
};

} // namespace API
