#include "field_impl.h"

namespace API {

InstanceFieldImpl::InstanceFieldImpl(
    Symlevel::String name, int ordinal, FieldFlags flags, Type* fieldType, Type* refType
)
    : name(name),
      ordinal(ordinal),
      fieldType(fieldType),
      refType(refType),
      flags(flags)
{
    ASSERTION(ordinal <= refType->FieldsNum(), "wrong ordinal value");
}

std::optional<uint32_t> InstanceFieldImpl::Offset()
{
    if (offset.has_value()) {
        return offset.value();
    }

    if (refType.has_value()) {
        offset = refType.value()->GetFieldOffset(Ordinal());
        return offset.value();
    }

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
