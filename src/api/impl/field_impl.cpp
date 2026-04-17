#include "field_impl.h"

using namespace Symlevel::Terms;

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

uint32_t InstanceFieldImpl::Offset()
{
    if (!offset.has_value()) {
        offset = refType->GetFieldOffset(Ordinal());
    }
    return offset.value();
}

StaticFieldImpl::StaticFieldImpl(
    uintptr_t location, Symlevel::String name, FieldFlags flags, Type* fieldType, Type* refType
)
    : location(location),
      name(name),
      flags(flags),
      fieldType(fieldType),
      refType(refType)
{}

} // namespace API
