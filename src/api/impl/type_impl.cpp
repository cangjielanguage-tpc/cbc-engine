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
        return static_cast<MRTExport::type_info_t*>(typeInfo.value())->type_info_name;
    } else {
        ASSERTION(false, "Support for generic types");
    }
}

} // namespace API
