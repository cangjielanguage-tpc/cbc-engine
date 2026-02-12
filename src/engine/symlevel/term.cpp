#include "term.h"


namespace Symlevel
{

Term* Term::Parse(IO::FileId fileId, IO::StreamFileReader& reader)
{
    auto termStr = String::Parse(fileId, reader);
    auto idx = reader.ReadU32();
    return new Term(fileId, termStr, idx);
}

} // namespace Symlevel
