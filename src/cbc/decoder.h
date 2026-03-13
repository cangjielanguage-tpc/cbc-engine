#ifndef CBC_DECODER_H
#define CBC_DECODER_H

#include "utils/assertion.h"
#include <cstdint>
#include <cstring>
#include <tuple>

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

template<typename... ts>
struct ByteReaderM;

template<typename... Ts>
struct ByteReaderM_;

template<typename... Ts>
struct ByteReaderM {
public:
	ByteReader& reader;
    ::std::tuple<Ts...> data;

	ByteReaderM(ByteReader& rreader) : reader(rreader), data() {}
	ByteReaderM(ByteReader& rreader, ::std::tuple<Ts...>&& base) : reader(rreader), data(::std::move(base)) {}
	ByteReaderM(const ByteReaderM<Ts...>&) = delete;
	ByteReaderM<Ts...>& operator=(const ByteReaderM<Ts...>&) = delete;

    template<typename T = uint8_t>
    auto Read4() && -> decltype(auto) {
        auto val = reader.Read8();
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(T(static_cast<uint8_t>((val >> 4) & 0xF))));
		return ByteReaderM_(reader, ::std::move(val & 0xF), ::std::move(new_data));
    }

    template<typename T = uint8_t>
    auto Read8() && -> decltype(auto) {
        auto val = T(reader.Read8());
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(val));
        return ByteReaderM_(reader, 0, ::std::move(new_data));
    }

    template<typename T = uint16_t>
    auto Read16() && -> decltype(auto) {
        auto val = T(reader.Read16());
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(val));
        return ByteReaderM_(reader, 0, ::std::move(new_data));
    }

    template<typename T = uint32_t>
    auto Read32() && -> decltype(auto) {
        auto val = T(reader.Read32());
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(val));
        return ByteReaderM_(reader, 0, ::std::move(new_data));
    }
    template<typename T = uint64_t>
    auto Read64() && -> decltype(auto) {
        auto val = T(reader.Read64());
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(val));
        return ByteReaderM_(reader, 0, ::std::move(new_data));
    }

	auto Get() && -> decltype(auto) {
		return ::std::move(data);
	}

};

template<typename... Ts>
struct ByteReaderM_ {
public:
	ByteReader& reader;
	uint8_t last;
    ::std::tuple<Ts...> data;

	ByteReaderM_(ByteReader& rreader) : reader(rreader) {}
	ByteReaderM_(ByteReader& rreader, uint8_t alast, ::std::tuple<Ts...>&& base) : reader(rreader), last(::std::move(alast)), data(::std::move(base)) {}
	ByteReaderM_(const ByteReaderM_<Ts...>&) = delete;
	ByteReaderM_<Ts...>& operator=(const ByteReaderM_<Ts...>&) = delete;

    template<typename T = uint8_t>
    auto Read4() && -> decltype(auto) {
        auto new_data = ::std::tuple_cat(data, ::std::make_tuple(T(last)));
		return ByteReaderM(reader, ::std::move(new_data));
    }

	auto Get() && -> decltype(auto) {
		return ::std::move(data);
	}

};

} // namespace Decoder

#endif // CBC_DECODER_H
