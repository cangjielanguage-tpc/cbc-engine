#include "stream_file_reader.h"
#include "utils/lebencodings.h"

namespace IO {

uint32_t StreamFileReader::ReadULEB()
{
    uint64_t val  = ReadLongULEB();
    uint32_t val0 = static_cast<uint32_t>(val);
    ASSERT(val0 == val);
    return val0;
}

int32_t StreamFileReader::ReadSLEB()
{
    int64_t val  = ReadLongSLEB();
    int32_t val0 = static_cast<int32_t>(val);
    ASSERT(val0 == val);
    return val0;
}

uint64_t StreamFileReader::ReadLongULEB()
{
    char buffer[LEB::MAX_SIZE];
    char* p = buffer;

    size_t peekLength = file.Peek(buffer, position, sizeof(buffer));

    auto result  = LEB::DecodeULEB(&p, p + peekLength);
    position    += p - buffer;

    return result;
}

int64_t StreamFileReader::ReadLongSLEB()
{
    char buffer[LEB::MAX_SIZE];
    char* p = buffer;

    size_t peekLength = file.Peek(buffer, position, sizeof(buffer));

    auto result  = LEB::DecodeSLEB(&p, p + peekLength);
    position    += p - buffer;

    return result;
}

} // namespace IO
