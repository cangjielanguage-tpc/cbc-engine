#include <utility>

#include "emitter.h"

namespace Cbc {
namespace Emitter {

size_t Segment::Position() const {
    return data.size();
}

void Segment::AddW8(uint32_t value) {
    assertion((value & 0xff) == value, "Out of bounds");
    data.push_back((uint8_t) ((value >> 0)  & 0xff));
}

void Segment::AddW16(uint32_t value) {
    assertion((value & 0xffff) == value, "Out of bounds");
    data.push_back((uint8_t) ((value >> 0)  & 0xff));
    data.push_back((uint8_t) ((value >> 8)  & 0xff));
}

void Segment::AddW32(uint32_t value) {
    data.push_back((uint8_t) ((value >> 0)  & 0xff));
    data.push_back((uint8_t) ((value >> 8)  & 0xff));
    data.push_back((uint8_t) ((value >> 16) & 0xff));
    data.push_back((uint8_t) ((value >> 24) & 0xff));
}

void Segment::AddW64(uint64_t value) {
    data.push_back((uint8_t) ((value >> 0)  & 0xff));
    data.push_back((uint8_t) ((value >> 8)  & 0xff));
    data.push_back((uint8_t) ((value >> 16) & 0xff));
    data.push_back((uint8_t) ((value >> 24) & 0xff));
    data.push_back((uint8_t) ((value >> 32) & 0xff));
    data.push_back((uint8_t) ((value >> 40) & 0xff));
    data.push_back((uint8_t) ((value >> 48) & 0xff));
    data.push_back((uint8_t) ((value >> 56) & 0xff));
}

SegmentSnapshot Segment::Snapshot() const {
    return SegmentSnapshot {
        .dataSize = data.size(),
    };
}

void Segment::Apply(SegmentSnapshot snapshot) {
    assertion(snapshot.dataSize <= data.size(), "Inconsistent snapshot");
    data.resize(snapshot.dataSize);
}

std::vector<uint8_t> Segment::Finish() {
    return std::exchange(this->data, {});
}

} // namespace Emitter
} // namespace Cbc
