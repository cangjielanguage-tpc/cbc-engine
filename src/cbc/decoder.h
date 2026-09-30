#ifndef CBC_DECODER_H
#define CBC_DECODER_H

#include "utils/assertion.h"
#include "utils/lebencodings.h"
#include <cstdint>
#include <cstring>
#include <tuple>
#include <vector>

namespace Decoder {

/// The ByteReader provides sequential access to a contiguous block of memory.
/// It is designed for low-level parsing tasks, such as instruction decoding.
/// It maintains a current cursor position and bounds (start/end) to prevent
/// buffer overflows in Debug builds.
///
/// \note This class does not own the memory it reads.

class FatByteReader {
public:
    FatByteReader(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor), start(_start), end(_end) {}

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

    inline uint64_t ReadSLEB()
    {
        return static_cast<uint64_t>(LEB::DecodeSLEB(reinterpret_cast<char**>(&cursor), reinterpret_cast<char*>(end)));
    }

    inline uint64_t ReadULEB()
    {
        return static_cast<uint64_t>(LEB::DecodeULEB(reinterpret_cast<char**>(&cursor), reinterpret_cast<char*>(end)));
    }

    inline uint32_t PeekOpcode()
    {
        StrictBoundCheck(cursor);
        return (uint32_t)*cursor;
    }

    inline uint8_t* Cursor() { return cursor; }

    inline bool EndOfMem(uint8_t* memEnd) { return cursor >= memEnd; }

    inline bool IsEndReached() { return cursor >= end; }

    uint8_t* Start() { return start; }

    uint8_t* End() { return end; }

private:
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

    uint8_t* cursor;
    uint8_t* start;
    uint8_t* end;
};

/// Bit-granular cursor over a byte-aligned buffer. Reads bits MSB-first.
/// Used for decoding tiered varint argument lists in call instructions.
class BitCursor {
public:
    explicit BitCursor(uint8_t* start) : start(start), p(start), bit(0) {}

    uint8_t r1()
    {
        uint8_t val = (*p >> (7 - bit)) & 1;
        bit++;
        if (bit == 8) {
            p++;
            bit = 0;
        }
        return val;
    }

    uint64_t read(int n)
    {
        uint64_t val = 0;
        for (int i = 0; i < n; i++) {
            val = (val << 1) | r1();
        }
        return val;
    }

    size_t BytesConsumed() const { return p - start; }

private:
    uint8_t* start;
    uint8_t* p;
    int bit;
};

/// Decode a tiered (bit-oriented) unsigned varint from a BitCursor.
/// Tier payload bits: {3, 3, 7, 15, 36}; every tier except the last is
/// preceded by a continuation bit. Returns the decoded value.
inline uint64_t ReadTieredVarInt(BitCursor& bc)
{
    static constexpr int payloadBits[5] = {3, 3, 7, 15, 36};
    uint64_t value = 0;
    for (int tier = 0;; ++tier) {
        bool last = (tier == 4) || bc.r1() == 0;
        value     = (value << payloadBits[tier]) | bc.read(payloadBits[tier]);
        if (last)
            return value;
    }
}

/// Decode the argument list from a FatByteReader. Reads tiered varints
/// (each with a +1 shift from the encoder) until a 0 value terminates the
/// list. Advances the reader past the zero-padded bit stream.
/// Returns the raw encoded values (1..14 = IReg, 15..30 = FReg, 31+ = slot).
inline std::vector<uint32_t> DecodeCallArgs(FatByteReader& reader)
{
    auto start = reader.Cursor();
    BitCursor bc(start);
    std::vector<uint32_t> args;
    for (;;) {
        uint64_t v = ReadTieredVarInt(bc);
        if (v == 0)
            break;
        args.push_back(static_cast<uint32_t>(v));
    }
    reader.Advance(static_cast<int64_t>(bc.BytesConsumed()));
    return args;
}

class UncheckedByteReader {
public:
    UncheckedByteReader(uint8_t* _cursor) : cursor(_cursor) {}

    void Advance(int64_t delta) { cursor += delta; }

    template <typename T> inline void ReadTo(T* target)
    {
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

    inline uint32_t PeekOpcode() { return (uint32_t)*cursor; }

    inline uint8_t* Cursor() { return cursor; }

    inline bool EndOfMem(uint8_t* memEnd) { return cursor >= memEnd; }

private:
    uint8_t* cursor;
};

#if !defined(NDEBUG)
struct ByteReader : public FatByteReader {
    ByteReader(uint8_t* cursor, uint8_t* start, uint8_t* end) : FatByteReader(cursor, start, end) {}
};
#else
struct ByteReader : public UncheckedByteReader {
    ByteReader(uint8_t* cursor, uint8_t* start, uint8_t* end) : UncheckedByteReader(cursor) {}
};
#endif // defined(NDEBUG)

template <typename... ts> struct ByteReaderM;

template <typename... Ts> struct ByteReaderM_;

template <typename... Ts> struct ByteReaderM {
public:
    ByteReader& reader;
    std::tuple<Ts...> data;

    ByteReaderM(ByteReader& rreader) : reader(rreader), data() {}

    ByteReaderM(ByteReader& rreader, std::tuple<Ts...>&& base) : reader(rreader), data(std::move(base)) {}

    ByteReaderM(const ByteReaderM<Ts...>&)                   = delete;
    ByteReaderM<Ts...>& operator=(const ByteReaderM<Ts...>&) = delete;

    template <typename T = uint8_t> auto Read4() && -> decltype(auto)
    {
        auto val      = reader.Read8();
        auto new_data = std::tuple_cat(data, std::make_tuple(T(static_cast<uint8_t>((val >> 4) & 0xF))));
        return ByteReaderM_<Ts..., T>(reader, std::move(val & 0xF), std::move(new_data));
    }

    template <typename T = uint8_t> auto Read8() && -> decltype(auto)
    {
        auto val      = T(reader.Read8());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., T>(reader, std::move(new_data));
    }

    template <typename T = uint16_t> auto Read16() && -> decltype(auto)
    {
        auto val      = T(reader.Read16());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., T>(reader, std::move(new_data));
    }

    template <typename T = uint32_t> auto Read32() && -> decltype(auto)
    {
        auto val      = T(reader.Read32());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., T>(reader, std::move(new_data));
    }

    template <typename T = uint64_t> auto Read64() && -> decltype(auto)
    {
        auto val      = T(reader.Read64());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., T>(reader, std::move(new_data));
    }

    auto Get() && -> decltype(auto) { return std::move(data); }
};

template <typename... Ts> struct ByteReaderM_ {
public:
    ByteReader& reader;
    uint8_t last;
    std::tuple<Ts...> data;

    ByteReaderM_(ByteReader& rreader) : reader(rreader) {}

    ByteReaderM_(ByteReader& rreader, uint8_t alast, std::tuple<Ts...>&& base)
        : reader(rreader),
          last(std::move(alast)),
          data(std::move(base))
    {}

    ByteReaderM_(const ByteReaderM_<Ts...>&)                   = delete;
    ByteReaderM_<Ts...>& operator=(const ByteReaderM_<Ts...>&) = delete;

    template <typename T = uint8_t> auto Read4() && -> decltype(auto)
    {
        auto new_data = std::tuple_cat(data, std::make_tuple(T(last)));
        return ByteReaderM<Ts..., T>(reader, std::move(new_data));
    }

    auto Get() && -> decltype(auto) { return std::move(data); }
};

} // namespace Decoder

#endif // CBC_DECODER_H
