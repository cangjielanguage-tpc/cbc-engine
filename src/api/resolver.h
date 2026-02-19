#pragma once

#include "field.h"
#include "index.h"
#include "method.h"
#include "term.h"
#include "type.h"
#include <optional>

namespace API {

class Resolver {
public:
    virtual Type* Resolve(Index<Type> index) = 0;

    virtual Term* Resolve(Index<Term> index) = 0;

    virtual Method* Resolve(Index<Method> index) = 0;

    virtual InstanceField* Resolve(Index<InstanceField> index) = 0;

    virtual StaticField* Resolve(Index<StaticField> index) = 0;

    virtual std::optional<Type*> Resolve(Term* term) = 0;

    virtual std::optional<Type*> TypeOf(Term* term) = 0;
};

} // namespace API
