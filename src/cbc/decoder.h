#ifndef CBC_DECODER_H
#define CBC_DECODER_H

#include <cstring>
#include <type_traits>

#include "isa.h"

namespace Decoder {

/// The ByteReader provides sequential access to a contiguous block of memory.
/// It is designed for low-level parsing tasks, such as instruction decoding.
/// It maintains a current cursor position and bounds (start/end) to prevent
/// buffer overflows in Debug builds.
///
/// \note This class does not own the memory it reads.
class ByteReader {
public:

#if defined(NDEBUG)
	Stream(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor), start(_start), end(_end) {}
#else
	ByteReader(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor) {}
#endif // defined(NDEBUG)

	void Advance(int32_t delta) {
		cursor += delta;
		BoundCheck(cursor);
	}

	template <typename T>
	inline void ReadTo(T *target) {
		BoundCheck(cursor + sizeof(T));
		memcpy(target, cursor, sizeof(T));
		cursor += sizeof(T);
	}

	template <typename T>
	inline T Read() {
		T v;
		ReadTo(&v);
		return v;
	}

	inline uint8_t Read8() {
		return Read<uint8_t>();
	}

	inline uint16_t Read16() {
		return Read<uint16_t>();
	}

	inline uint32_t Read32() {
		return Read<uint32_t>();
	}

	inline uint64_t Read64() {
		return Read<uint64_t>();
	}

	inline uint32_t PeekOpcode() {
		StrictBoundCheck(cursor);
		return (uint32_t) *cursor;
	}

	inline uint8_t *Cursor() {
		return cursor;
	}

private:
#if defined(NDEBUG)
	void BoundCheck(uint8_t *p) {
		ASSERTION(this->start <= p, "underflow");
		ASSERTION(p <= this->end, "overflow");
	}

	void StrictBoundCheck(uint8_t *p) {
		ASSERTION(this->start <= p, "underflow");
		ASSERTION(p < this->end, "overflow");
	}
#else
	inline void BoundCheck(uint8_t *p) {}
	inline void StrictBoundCheck(uint8_t *p) {}
#endif // defined(NDEBUG)
			 //
	uint8_t* cursor;

#if defined(NDEBUG)
	uint8_t* start;
	uint8_t* end;
#endif // defined(NDEBUG)
};

struct B2rr {
	uint32_t opcode;
	union {
		uint32_t xreg;
		uint32_t dst;
	};
	union {
		uint32_t yreg;
		uint32_t src;
	};

	inline Cbc::IReg IX() {
		return Cbc::IReg(xreg);
	}

	inline Cbc::IReg IY() {
		return Cbc::IReg(yreg);
	}

	inline Cbc::IReg Idst() {
		return Cbc::IReg(dst);
	}

	inline Cbc::IReg Isrc() {
		return Cbc::IReg(src);
	}

	static inline B2rr Decode(ByteReader *stream) {
		uint32_t opcode = (uint32_t) stream->Read8();
		uint32_t b = (uint32_t) stream->Read8();
		return B2rr {
			.opcode = opcode,
			.xreg = b & 0xf,
			.yreg = (b >> 4) & 0xf,
		};
	}
};

struct B2rrd8 {
	B2rr rr;
	uint8_t byte;

	static inline B2rrd8 Decode(ByteReader *stream) {
		B2rrd8 res;
		res.rr = B2rr::Decode(stream);
		stream->ReadTo(&res.byte);
		return res;
	}
};

struct ExtBrr {
    using CC = Cbc::Format::CC;
    B2rr rr;
    CC cc;
    uint16_t offsetValue;

	static inline ExtBrr Decode(ByteReader *stream) {
		B2rr rr = B2rr::Decode(stream);
        auto cc = CC(stream->Read8());
        uint16_t offsetVal = stream->Read16();
		return ExtBrr {
            .rr = rr,
            .cc = cc,
            .offsetValue = offsetVal,
        };
	}
};

} // namespace Decoder


#endif // CBC_DECODER_H
