#include "engine/image/io/random_access_file.h"
#include "utils/assertion.h"

void IO::RandomAccessFile::Read(char* array, size_t position, size_t length) const
{
    if (this->Peek(array, position, length) != length) {
        FATAL("Out of memory");
    }
}

template <typename T> static T ReadValue(IO::RandomAccessFile* file, size_t position)
{
    T value;
    file->Read(reinterpret_cast<char*>(&value), position, sizeof(T));
    return value;
}

uint8_t IO::RandomAccessFile::ReadU8(size_t position) { return ReadValue<uint8_t>(this, position); }

uint16_t IO::RandomAccessFile::ReadU16(size_t position) { return ReadValue<uint16_t>(this, position); }

uint32_t IO::RandomAccessFile::ReadU32(size_t position) { return ReadValue<uint32_t>(this, position); }

int32_t IO::RandomAccessFile::ReadS32(size_t position) { return ReadValue<int32_t>(this, position); }

uint64_t IO::RandomAccessFile::ReadU64(size_t position) { return ReadValue<uint64_t>(this, position); }
