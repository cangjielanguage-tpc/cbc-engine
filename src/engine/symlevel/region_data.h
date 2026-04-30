#pragma once

#include "io/offset_pool.h"
#include "references.h"

namespace Symlevel {

class RegionData {
public:
    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    Offset<MethodReference> Query(Engine::Session& session, RefId<MethodReference> index) const;
    Offset<FieldReference> Query(Engine::Session& session, RefId<FieldReference> index) const;
    Offset<Term> Query(Engine::Session& session, RefId<Term> index) const;

    IO::OffsetPool<MethodReference> const& MethodReferencesOffsets() const;
    IO::OffsetPool<FieldReference> const& FieldReferencesOffsets() const;
    IO::OffsetPool<Term> const& TermsOffsets() const;

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
