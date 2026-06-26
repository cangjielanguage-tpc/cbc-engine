#include "engine/symlevel/io/random_access_file.h"
#include "utils/assertion.h"

void IO::RandomAccessFile::Read(char* array, size_t position, size_t length) const
{
    if (this->Peek(array, position, length) != length) {
        FATAL("Out of memory");
    }
}
