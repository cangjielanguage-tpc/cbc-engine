#ifndef CBC_EMITTER_SEGMENT_H
#define CBC_EMITTER_SEGMENT_H

#include <cstdint>
#include <vector>

namespace Cbc {
namespace Emitter {

struct SegmentSnapshot {
    size_t dataSize;
};

class Segment {
public:
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

    SegmentSnapshot Snapshot() const;
    void Apply(SegmentSnapshot snapshot);

    std::vector<uint8_t> Finish();
private:
    std::vector<uint8_t> data;
};

}
}

#endif // CBC_EMITTER_SEGMENT_H
