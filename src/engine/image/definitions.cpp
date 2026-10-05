#include "engine/image/cbc_file.h"
#include "engine/image/flags.h"

namespace Image {

MethodRefFlags MethodDefinition::GetABIFlags() const
{
    MethodRefFlags flags;
    auto defFlags = content.flags;
    if (defFlags.Is(MethodFlag::SRET))
        flags = flags.Or(MethodRefFlag::SRET);
    if (defFlags.Is(MethodFlag::HAS_OUTER_TI))
        flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (content.arity > 0)
        flags = flags.Or(MethodRefFlag::HAS_FTVARS);

    return flags;
}

} // namespace Image
