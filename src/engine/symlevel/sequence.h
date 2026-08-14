#pragma once

#include "engine/symlevel/io/file_id.h"

#include <cstdint>

namespace Symlevel {

template <typename T> struct OffsetSequence {
    OffsetSequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos)
    {}

    OffsetSequence() : file(0), startPos(0), endPos(0) {}

    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

template <typename T> struct RefSequence {
    RefSequence(IO::FileId file, uint32_t startPos, uint32_t endPos) : file(file), startPos(startPos), endPos(endPos) {}

    RefSequence() : file(0), startPos(0), endPos(0) {}

    IO::FileId file;
    uint32_t startPos;
    uint32_t endPos;
};

} // namespace Symlevel
