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
    ByteReader(uint8_t* _cursor, uint8_t* _start, uint8_t* _end) : cursor(_cursor), start(_start), end(_end) {}
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
    uint32_t const opcode;
    uint32_t const xreg : 4;
    uint32_t const yreg : 4;

    inline Cbc::IReg IX() const {
        return Cbc::IReg(xreg);
    }

    inline Cbc::IReg IY() const {
        return Cbc::IReg(yreg);
    }

    inline Cbc::IReg Idst() const {
        return Cbc::IReg(xreg);
    }

    inline Cbc::IReg Isrc() const {
        return Cbc::IReg(yreg);
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

struct B2hr {
    uint32_t const opcode;
    uint32_t const imm : 4;
    uint32_t const xreg : 4;

    inline Cbc::IReg IX() const {
        return Cbc::IReg(xreg);
    }

    inline Cbc::IReg Idst() const {
        return Cbc::IReg(xreg);
    }

    static inline B2hr Decode(ByteReader *stream) {
        uint32_t opcode = (uint32_t) stream->Read8();
        uint32_t b = (uint32_t) stream->Read8();
        return B2hr {
            .opcode = opcode,
            .imm = b & 0xf,
            .xreg = (b >> 4) & 0xf
        };
    }
};

struct B2rrd8 {
    B2rr const rr;
    uint8_t const imm;

    static inline B2rrd8 Decode(ByteReader *stream) {
        auto rr = B2rr::Decode(stream);
        auto byte = stream->Read8();
        return B2rrd8 {
            .rr = rr,
            .imm = byte
        };
    }
};

struct B2xrI {
    uint32_t const opcode;
    uint32_t const opx : 4;
    uint32_t const reg : 4;
    uint16_t const imm;

    inline Cbc::IReg Ireg() const {
        return Cbc::IReg(reg);
    }

    static inline B2xrI Decode(ByteReader *stream) {
        uint32_t opcode = (uint32_t) stream->Read8();
        uint32_t b = (uint32_t) stream->Read8();
        uint16_t imm = stream->Read16();
        return B2xrI {
            .opcode = opcode,
            .opx = b & 0xf,
            .reg = (b >> 4) & 0xf,
            .imm = imm,
        };
    }
};

struct B3xrrr {
    uint32_t const opcode;
    uint32_t const opx : 4;
    uint32_t const regx : 4;
    uint32_t const regy : 4;
    uint32_t const regw : 4;

    inline Cbc::IReg IRegX() const {
        return Cbc::IReg(regx);
    }

    inline Cbc::IReg IRegY() const {
        return Cbc::IReg(regy);
    }

    inline Cbc::IReg IRegW() const {
        return Cbc::IReg(regw);
    }

    static inline B3xrrr Decode(ByteReader *stream) {
        uint32_t opcode = (uint32_t) stream->Read8();
        uint32_t xr = (uint32_t) stream->Read8();
        uint32_t rr = (uint32_t) stream->Read8();
        return B3xrrr {
            .opcode = opcode,
            .opx = xr & 0xf,
            .regx = (xr >> 4) & 0xf,
            .regy = rr & 0xf,
            .regw = (rr >> 4) & 0xf
        };
    }
};

struct B3xrrtiK {
    uint32_t const opcode;
    uint32_t const opx : 4;
    uint32_t const regx : 4;
    uint32_t const regy : 4;
    uint32_t const t4 : 4;
    uint16_t const imm;

    inline Cbc::IReg IRegX() const {
        return Cbc::IReg(regx);
    }

    inline Cbc::IReg IRegY() const {
        return Cbc::IReg(regy);
    }

    inline uint64_t Imm() const {
        ASSERTION(t4 == 0, "Decoding imm with ival(t4, imm) not implemented");
        return imm;
    }

    static inline B3xrrtiK Decode(ByteReader *stream) {
        uint32_t opcode = (uint32_t) stream->Read8();
        uint32_t xr = (uint32_t) stream->Read8();
        uint32_t rt = (uint32_t) stream->Read8();
        uint16_t imm = 0;
        auto kk = (opcode >> 3) & 0b11;
        switch (kk)
        {
            case 0b00: break;
            case 0b01: imm = (uint16_t) stream->Read8(); break;
            case 0b10: imm = stream->Read16(); break;
            default: ASSERTION(false, "Unexpected kk"); break;
        }
        return B3xrrtiK {
            .opcode = opcode,
            .opx = xr & 0xf,
            .regx = (xr >> 4) & 0xf,
            .regy = rt & 0xf,
            .t4 = (rt >> 4) & 0xf,
            .imm = imm
        };
    }
};

struct B3xrrkI {
    uint32_t const opcode;
    uint32_t const opx : 4;
    uint32_t const regx : 4;
    uint32_t const regy : 4;
    uint32_t const k4 : 4;
    uint16_t const imm;

    inline Cbc::IReg IRegX() const {
        return Cbc::IReg(regx);
    }

    inline Cbc::IReg IRegY() const {
        return Cbc::IReg(regy);
    }

    static inline B3xrrkI Decode(ByteReader *stream) {
        uint32_t opcode = (uint32_t) stream->Read8();
        uint32_t xr = (uint32_t) stream->Read8();
        uint32_t rk = (uint32_t) stream->Read8();
        uint16_t imm = stream->Read16();
        return B3xrrkI {
            .opcode = opcode,
            .opx = xr & 0xf,
            .regx = (xr >> 4) & 0xf,
            .regy = rk & 0xf,
            .k4 = (rk >> 4) & 0xf,
            .imm = imm
        };
    }
};

struct ExtBrr {
    using CC = Cbc::Format::CC;
    B2rr const rr;
    uint16_t const offsetValue;

    static inline ExtBrr Decode(ByteReader *stream) {
        B2rr rr = B2rr::Decode(stream);
        uint16_t offsetVal = stream->Read16();
        return ExtBrr {
            .rr = rr,
            .offsetValue = offsetVal,
        };
    }
};

} // namespace Decoder


#endif // CBC_DECODER_H
