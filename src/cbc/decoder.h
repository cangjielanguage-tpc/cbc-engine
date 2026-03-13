#ifndef CBC_DECODER_H
#define CBC_DECODER_H

#include "utils/assertion.h"
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace Decoder {

/// The ByteReader provides sequential access to a contiguous block of memory.
/// It is designed for low-level parsing tasks, such as instruction decoding.
/// It maintains a current cursor position and bounds (start/end) to prevent
/// buffer overflows in Debug builds.
///
/// \note This class does not own the memory it reads.
class ByteReader {
public:
#if !defined(NDEBUG)
    ByteReader(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor), start(_start), end(_end) {}
#else
    ByteReader(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor) {}
#endif // defined(NDEBUG)

    void Advance(int64_t delta)
    {
        cursor += delta;
        BoundCheck(cursor);
    }

    template <typename T> inline void ReadTo(T* target)
    {
        BoundCheck(cursor + sizeof(T));
        memcpy(target, cursor, sizeof(T));
        cursor += sizeof(T);
    }

    template <typename T> inline T Read()
    {
        T v;
        ReadTo(&v);
        return v;
    }

    inline uint8_t Read8() { return Read<uint8_t>(); }

    inline uint16_t Read16() { return Read<uint16_t>(); }

    inline uint32_t Read32() { return Read<uint32_t>(); }

    inline uint64_t Read64() { return Read<uint64_t>(); }

    inline uint32_t PeekOpcode()
    {
        StrictBoundCheck(cursor);
        return (uint32_t)*cursor;
    }

    inline uint8_t* Cursor() { return cursor; }

    inline bool EndOfMem(uint8_t* memEnd) { return cursor >= memEnd; }

private:
#if !defined(NDEBUG)
    void BoundCheck(uint8_t* p)
    {
        ASSERTION(this->start <= p, "underflow");
        ASSERTION(p <= this->end, "overflow");
    }

    void StrictBoundCheck(uint8_t* p)
    {
        ASSERTION(this->start <= p, "underflow");
        ASSERTION(p < this->end, "overflow");
    }
#else
    inline void BoundCheck(uint8_t* p) {}

    inline void StrictBoundCheck(uint8_t* p) {}
#endif // defined(NDEBUG)
       //
    uint8_t* cursor;

#if !defined(NDEBUG)
    uint8_t* start;
    uint8_t* end;
#endif // defined(NDEBUG)
};

} // namespace Decoder

#endif // CBC_DECODER_H
