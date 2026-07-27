#include <utility>

#include "emitter.h"

namespace Cbc {
namespace Emitter {

int32_t Segment::Pos() const { return (int32_t)data.size(); }

void Segment::AddW8(uint32_t value)
{
    ASSERT((value & 0xff) == value);
    data.push_back((uint8_t)((value >> 0) & 0xff));
    ASSERT(data.size() < INT32_MAX);
}

void Segment::AddW16(uint32_t value)
{
    ASSERT((value & 0xffff) == value);
    data.push_back((uint8_t)((value >> 0) & 0xff));
    data.push_back((uint8_t)((value >> 8) & 0xff));
    ASSERT(data.size() < INT32_MAX);
}

void Segment::AddW32(uint32_t value)
{
    data.push_back((uint8_t)((value >> 0) & 0xff));
    data.push_back((uint8_t)((value >> 8) & 0xff));
    data.push_back((uint8_t)((value >> 16) & 0xff));
    data.push_back((uint8_t)((value >> 24) & 0xff));
    ASSERT(data.size() < INT32_MAX);
}

void Segment::AddW64(uint64_t value)
{
    data.push_back((uint8_t)((value >> 0) & 0xff));
    data.push_back((uint8_t)((value >> 8) & 0xff));
    data.push_back((uint8_t)((value >> 16) & 0xff));
    data.push_back((uint8_t)((value >> 24) & 0xff));
    data.push_back((uint8_t)((value >> 32) & 0xff));
    data.push_back((uint8_t)((value >> 40) & 0xff));
    data.push_back((uint8_t)((value >> 48) & 0xff));
    data.push_back((uint8_t)((value >> 56) & 0xff));
    ASSERT(data.size() < INT32_MAX);
}

void Segment::SetW8(size_t pos, uint32_t value)
{
    ASSERTION((value & 0xff) == value, "Out of bounds");
    ASSERTION(pos < data.size(), "Out of bounds");
    data[pos + 0] = ((uint8_t)((value >> 0) & 0xff));
}

void Segment::SetW16(size_t pos, uint32_t value)
{
    ASSERTION((value & 0xffff) == value, "Out of bounds");
    ASSERTION(pos + 1 < data.size(), "Out of bounds");
    data[pos + 0] = ((uint8_t)((value >> 0) & 0xff));
    data[pos + 1] = ((uint8_t)((value >> 8) & 0xff));
    ASSERT(data.size() < INT32_MAX);
}

void Segment::SetW32(size_t pos, uint32_t value)
{
    ASSERTION(pos + 3 < data.size(), "Out of bounds");
    data[pos + 0] = ((uint8_t)((value >> 0) & 0xff));
    data[pos + 1] = ((uint8_t)((value >> 8) & 0xff));
    data[pos + 2] = ((uint8_t)((value >> 16) & 0xff));
    data[pos + 3] = ((uint8_t)((value >> 24) & 0xff));
}

void Segment::SetW64(size_t pos, uint64_t value)
{
    ASSERTION(pos + 7 < data.size(), "Out of bounds");
    data[pos + 0] = ((uint8_t)((value >> 0) & 0xff));
    data[pos + 1] = ((uint8_t)((value >> 8) & 0xff));
    data[pos + 2] = ((uint8_t)((value >> 16) & 0xff));
    data[pos + 3] = ((uint8_t)((value >> 24) & 0xff));
    data[pos + 4] = ((uint8_t)((value >> 32) & 0xff));
    data[pos + 5] = ((uint8_t)((value >> 40) & 0xff));
    data[pos + 6] = ((uint8_t)((value >> 48) & 0xff));
    data[pos + 7] = ((uint8_t)((value >> 56) & 0xff));
}

SegmentSnapshot Segment::Snapshot() const
{
    return SegmentSnapshot {
        .dataSize = data.size(),
    };
}

void Segment::Apply(SegmentSnapshot snapshot)
{
    ASSERTION(snapshot.dataSize <= data.size(), "Inconsistent snapshot");
    data.resize(snapshot.dataSize);
}

Utils::Vector<uint8_t> Segment::Finish() { return std::exchange(this->data, {}); }

Segment::View Segment::At(size_t pos) { return View(*this, pos); }

void Segment::View::AddW8(uint32_t value)
{
    segment.SetW8(position, value);
    position += 1;
}

void Segment::View::AddW16(uint32_t value)
{
    segment.SetW16(position, value);
    position += 2;
}

void Segment::View::AddW32(uint32_t value)
{
    segment.SetW32(position, value);
    position += 4;
}

void Segment::View::AddW64(uint64_t value)
{
    segment.SetW64(position, value);
    position += 8;
}

} // namespace Emitter
} // namespace Cbc
