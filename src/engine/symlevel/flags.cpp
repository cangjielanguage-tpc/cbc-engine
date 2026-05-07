#include "flags.h"
#include "utils/ostream.h"

namespace Symlevel {

Stream::Output& operator<<(Stream::Output& stream, TypeFlags flags)
{
    stream << flags.GetAccessKind() << " " << flags.GetTypeKind();
    for (TypeFlag flag : TypeFlag::values) {
        if (flags.Is(flag)) {
            stream << " " << flag;
        }
    }
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, MethodFlags flags)
{
    stream << flags.GetAccessKind();
    for (MethodFlag flag : MethodFlag::values) {
        if (flags.Is(flag)) {
            stream << " " << flag;
        }
    }
    return stream;
}



Stream::Output& operator<<(Stream::Output& stream, FieldFlags flags)
{
    auto prefix = "";
    for (FieldFlag flag : FieldFlag::values) {
        if (flags.Is(flag)) {
            stream << prefix << flag;
            prefix = " ";
        }
    }
    return stream;
}

Stream::Output& operator<<(Stream::Output& stream, FieldFlag flag)
{
    return stream << flag.ToString();
}

Stream::Output& operator<<(Stream::Output& stream, TypeFlag flag)
{
    return stream << flag.ToString();
}

Stream::Output& operator<<(Stream::Output& stream, MethodFlag flag)
{
    return stream << flag.ToString();
}

template <typename T>
static std::string Str(T const* t) {
    Stream::StringBuffer buf;
    buf << *t;
    return buf.ToString();
}

std::string TypeFlags::ToString() const {
    return Str(this);
}

std::string FieldFlags::ToString() const {
    return Str(this);
}

std::string MethodFlags::ToString() const {
    return Str(this);
}

}
