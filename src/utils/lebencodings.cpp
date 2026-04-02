#include "lebencodings.h"
#include "assertion.h"

namespace LEB {

uint64_t DecodeULEB(char** bytes, char const* end)
{
    if (*bytes >= end) {
        ASSERT(*bytes == end);
        return 0;
    }

    uint64_t result = 0;

    char* cursor = *bytes;
    int shift    = 0;
    int b        = 0;
    do {
        b       = *cursor;
        result |= static_cast<uint64_t>(b & 0x7f) << shift;
        shift  += 7;
        cursor++;
    } while ((b & 0x80) != 0 && cursor < end);

    *bytes = cursor;

    return result;
}

int64_t DecodeSLEB(char** bytes, char const* end)
{
    if (*bytes >= end) {
        ASSERT(*bytes == end);
        return 0;
    }

    uint64_t result = 0;

    char* cursor = *bytes;
    int shift    = 0;
    int b        = 0;
    do {
        b       = *cursor;
        result |= static_cast<uint64_t>(b & 0x7f) << shift;
        shift  += 7;
        cursor++;
    } while ((b & 0x80) != 0 && cursor < end);

    if (shift < 64 && ((b & 0x40) != 0)) {
        result |= static_cast<uint64_t>(-1) << shift;
    }

    *bytes = cursor;

    return static_cast<int64_t>(result);
}

void EncodeSLEB(int64_t value, char** bytes, char const* end)
{
    char* cursor = *bytes;
    while (cursor < end) {
        auto b    = value & 0x7f;
        auto sign = b & 0x40;
        value     = value >> 7;
        if (((value == 0) && (sign == 0)) || ((value == -1) && (sign != 0))) {
            *cursor = static_cast<char>(b);
            cursor++;
            break;
        } else {
            *cursor = static_cast<char>(0x80 | b);
            cursor++;
        }
    }
    *bytes = cursor;
}

void EncodeULEB(uint64_t value, char** bytes, char const* end)
{
    char* cursor = *bytes;
    while (cursor < end) {
        auto b = value & 0x7f;
        value  = value >> 7;
        if (value == 0) {
            *cursor = static_cast<char>(b);
            cursor++;
            break;
        } else {
            *cursor = static_cast<char>(0x80 | b);
            cursor++;
        }
    }
    *bytes = cursor;
}

} // namespace LEB
