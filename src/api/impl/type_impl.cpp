#include "type_impl.h"
#include "runtimesupport/runtime.h"

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

std::optional<TypeInfo> TypeImpl::GetTypeInfo() { return typeInfo; }

int TypeImpl::FieldsNum() { FATAL("not implemented"); }

uint32_t TypeImpl::GetFieldOffset(int ordinal)
{
    return 0;
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
        return "FIXME: refactor resolver api";
    } else {
        ASSERTION(false, "Support for generic types");
    }
}

} // namespace API
