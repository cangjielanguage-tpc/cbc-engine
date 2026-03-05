#pragma once

#include "index.h"
#include "offset.h"
#include "references.h"

namespace Symlevel {

class RegionData {
public:
    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    MethodReference queryMethod(Engine::Session& session, Index<MethodReference> index) const;
    FieldReference queryField(Engine::Session& session, Index<FieldReference> index) const;

private:
    RegionData(
        IO::FileId fileId,
        uint16_t methodIndexSize,
        uint32_t methodIndexOffset,
        uint16_t fieldIndexSize,
        uint32_t fieldIndexOffset,
        uint32_t termIndexSize,
        uint32_t termIndexOffset
    );

    IO::FileId fileId;

    uint16_t methodIndexSize;
    uint32_t methodIndexOffset;

    uint16_t fieldIndexSize;
    uint32_t fieldIndexOffset;

    uint32_t termIndexSize;
    uint32_t termIndexOffset;
};

} // namespace Symlevel
