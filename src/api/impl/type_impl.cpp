#include "type_impl.h"

using namespace Symlevel;

namespace API {

TypeImpl::TypeImpl(Term term) : term(term), typeInfo(std::nullopt)
{
    ASSERTION(term.GetLength() != 0, "Use only for generic types");
}

TypeImpl::TypeImpl(Term term, TypeInfo typeInfo) : term(term), typeInfo(typeInfo)
{
    ASSERTION(term.GetLength() == 0, "Use only for non-generic types");
}

Term* TypeImpl::AsTerm() { return &term; }

std::optional<TypeInfo> TypeImpl::GetTypeInfo() { return typeInfo; }

int TypeImpl::FieldsNum()
{
    DYN_TypeInfoT* ti = typeInfo.value();
    return ti->fieldNum;
}

uint32_t TypeImpl::GetFieldOffset(int ordinal)
{
    ASSERTION(typeInfo.has_value(), "cannot get field offset");

    DYN_TypeInfoT* ti = typeInfo.value();
    return ti->fieldOffsets[ordinal] + sizeof(DYN_TypeInfoT*);
}

int TypeImpl::FieldSize()
{
    ASSERTION(false, "Not implemented yet");
    return 0;
}

std::vector<int> TypeImpl::RefOffsets()
{
    ASSERTION(false, "Not implemented yet");
    return std::vector<int>();
}

TypeFlags TypeImpl::Flags()
{
    ASSERTION(false, "Not implemented yet");
    return TypeFlags();
}

std::string_view TypeImpl::FullName()
{
    if (term.GetLength() == 0) {
        return static_cast<DYN_TypeInfoT*>(typeInfo.value())->typeInfoName;
    } else {
        ASSERTION(false, "Support for generic types");
    }
}

} // namespace API
