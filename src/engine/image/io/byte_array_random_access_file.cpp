#include <cstring>

#include "byte_array_random_access_file.h"

namespace IO {

size_t ByteArrayRandomAccessFile::Peek(char* array, size_t position, size_t length) const
{
    if (array == nullptr || position >= data.Size()) {
        return 0;
    }

    size_t realEnd = std::min(position + length, data.Size());
    size_t realLen = realEnd - position;

    memcpy(array, data.Data() + position, realLen);

    return realLen;
};

} // namespace IO
