#pragma once

#include "engine/identifiers.h"
#include "io/offset_pool.h"
#include "references.h"

namespace Symlevel {

class RegionData {
    template <typename T> using OffsetId = Engine::Identifier<T>;

public:
    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    Offset<MethodReference> Query(Engine::Session& session, Index<MethodReference> index) const;
    Offset<FieldReference> Query(Engine::Session& session, Index<FieldReference> index) const;
    Offset<Term> Query(Engine::Session& session, Index<Term> index) const;

private:
    RegionData(
        IO::FileId fileId,
        IO::OffsetPool<MethodReference> methods,
        IO::OffsetPool<FieldReference> fields,
        IO::OffsetPool<Term> terms
    );

    IO::FileId fileId;

    IO::OffsetPool<MethodReference> methods;
    IO::OffsetPool<FieldReference> fields;
    IO::OffsetPool<Term> terms;
};

} // namespace Symlevel
