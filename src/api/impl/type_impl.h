#pragma once

#include "api/type.h"
#include "engine/symlevel/terms.h"
#include "utils/assertion.h"
#include <optional>

namespace API {
class TypeImpl : public Type {
    using Term = Symlevel::Terms::Term;

public:
    TypeImpl(Term term);
    TypeImpl(Term term, TypeInfo ti);

    Term* AsTerm() override;

    std::optional<TypeInfo> GetTypeInfo() override;

    int FieldSize() override;

    std::vector<int> RefOffsets() override;

    TypeFlags Flags() override;

    std::string_view FullName() override;

private:
    Term term;
    std::optional<TypeInfo> typeInfo;
};

} // namespace API
