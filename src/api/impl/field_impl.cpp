#include "field_impl.h"
#include <optional>

namespace API {

InstanceFieldImpl::InstanceFieldImpl(
    Symlevel::String name, int ordinal, FieldFlags flags, Type* fieldType, Type* refType
)
    : name(name),
      ordinal(ordinal),
      fieldType(fieldType),
      refType(refType),
      flags(flags)
{}

std::optional<uint32_t> InstanceFieldImpl::Offset()
{
    return std::nullopt;
}

StaticFieldImpl::StaticFieldImpl(
    uintptr_t location,
    Symlevel::String name,
    FieldFlags flags,
    std::optional<Type*> fieldType,
    std::optional<Type*> refType
)
    : location(location),
      name(name),
      flags(flags),
      fieldType(fieldType),
      refType(refType)
{}

} // namespace API
