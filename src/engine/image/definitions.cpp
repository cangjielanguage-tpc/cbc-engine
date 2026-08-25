#include "engine/decode/decoder.h"
#include "engine/image/access_kind.h"
#include "engine/image/flags.h"
#include "engine/image/reader.h"
#include <cstdint>
#include <optional>

namespace Image {

MethodRefFlags MethodDefinition::GetABIFlags() const
{
    MethodRefFlags flags;
    auto defFlags = content.flags;
    if (defFlags.Is(MethodFlag::SRET))
        flags = flags.Or(MethodRefFlag::SRET);
    if (defFlags.Is(MethodFlag::HAS_OUTER_TI))
        flags = flags.Or(MethodRefFlag::HAS_OUTER_TI);
    if (defFlags.Is(MethodFlag::HAS_THIS_TI))
        flags = flags.Or(MethodRefFlag::HAS_THIS_TI);
    if (defFlags.Is(MethodFlag::MUT))
        flags = flags.Or(MethodRefFlag::MUT);
    if (content.arity > 0)
        flags = flags.Or(MethodRefFlag::HAS_FTVARS);
    if (defFlags.Is(MethodFlag::REC_RECEIVER))
        flags = flags.Or(MethodRefFlag::REC_RECEIVER);
    if (defFlags.Is(MethodFlag::REF_RECEIVER))
        flags = flags.Or(MethodRefFlag::REF_RECEIVER);

    return flags;
}

} // namespace Image
