#include "string.h"


namespace Symlevel {

String* String::Parse(IO::FileId fileId, IO::StreamFileReader &reader)
{
    uint32_t length = reader.ReadULEB();

    std::string content(length, '\0');
    reader.Read(&content[0], length);

    return new String(std::move(fileId), std::move(content));
}


} // namespace Symlevel
