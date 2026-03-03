#include "stream_file_reader.h"

namespace IO {

// 10 bytes are enough to fit 64-bit ulebs
static constexpr auto BUFFER_SIZE = 10;

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
    char buffer[BUFFER_SIZE];

    size_t peekLength = file.Peek(buffer, position, BUFFER_SIZE);

    uint64_t result = 0;

    uint32_t idx = 0;
    int shift    = 0;
    int b        = 0;
    do {
        b       = buffer[idx++];
        result |= static_cast<uint64_t>(b & 0x7f) << shift;
        shift  += 7;
    } while ((b & 0x80) != 0 && idx < BUFFER_SIZE); // explicitly check for buffer size for safety

    position += idx;

    return result;
}

int64_t StreamFileReader::ReadLongSLEB()
{
    char buffer[BUFFER_SIZE];

    size_t peekLength = file.Peek(buffer, position, BUFFER_SIZE);

    uint64_t result = 0;

    uint32_t idx = 0;
    int shift    = 0;
    int b        = 0;
    do {
        b       = buffer[idx++];
        result |= static_cast<uint64_t>(b & 0x7f) << shift;
        shift  += 7;
    } while ((b & 0x80) != 0 && idx < BUFFER_SIZE); // explicitly check for buffer size for safety

    if (shift < 64 && ((b & 0x40) != 0)) {
        result |= static_cast<uint64_t>(-1) << shift;
    }

    position += static_cast<size_t>(idx);

    return static_cast<int32_t>(result);
}

} // namespace IO
