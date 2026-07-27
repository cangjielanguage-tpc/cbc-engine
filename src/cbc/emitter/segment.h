#ifndef CBC_EMITTER_SEGMENT_H
#define CBC_EMITTER_SEGMENT_H

#include "utils/vector.h"
#include <cstdint>

namespace Cbc {
namespace Emitter {

struct SegmentSnapshot {
    size_t dataSize;
};

class ByteBuffer {
public:
    virtual void AddW8(uint32_t value)  = 0;
    virtual void AddW16(uint32_t value) = 0;
    virtual void AddW32(uint32_t value) = 0;
    virtual void AddW64(uint64_t value) = 0;
};

class Segment : public ByteBuffer {
public:
    class View : public ByteBuffer {
    public:
        View(Segment& _segment, size_t _position) : segment(_segment), position(_position) {}

        void AddW8(uint32_t value);
        void AddW16(uint32_t value);
        void AddW32(uint32_t value);
        void AddW64(uint64_t value);

    private:
        Segment& segment;
        size_t position;
    };

    Segment() = default;
    int32_t Pos() const;
    void AddW8(uint32_t value);
    void AddW16(uint32_t value);
    void AddW32(uint32_t value);
    void AddW64(uint64_t value);

    void SetW8(size_t pos, uint32_t value);
    void SetW16(size_t pos, uint32_t value);
    void SetW32(size_t pos, uint32_t value);
    void SetW64(size_t pos, uint64_t value);
    View At(size_t pos);

    SegmentSnapshot Snapshot() const;
    void Apply(SegmentSnapshot snapshot);

    Utils::Vector<uint8_t> Finish();

private:
    Utils::Vector<uint8_t> data;
};

} // namespace Emitter
} // namespace Cbc

#endif // CBC_EMITTER_SEGMENT_H
