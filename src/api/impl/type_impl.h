#pragma once

#include "api/type.h"
#include "engine/terms.h"
#include <optional>

namespace API {

class TypeImpl : public Type {
    using Term = Engine::Term;

public:
    TypeImpl(Term term);
    TypeImpl(Term term, TypeInfo ti);

    std::optional<TypeInfo> GetTypeInfo() override;

    int FieldsNum() override;

    uint32_t GetFieldOffset(int ordinal) override;

    int FieldSize() override;

    std::vector<int> RefOffsets() override;

    TypeFlags Flags() override;

    std::string_view FullName() override;

private:
    Term term;
    std::optional<TypeInfo> typeInfo;
};

} // namespace API
