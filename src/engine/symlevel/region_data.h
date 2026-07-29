#pragma once

#include "io/offset_pool.h"
#include "references.h"
#include <cstdint>

namespace Symlevel {

class RegionData {
public:
    static constexpr uint32_t FIRST_NON_PRIMITIVE_TERM_ID = 20;

    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    Offset<MethodReference> Query(Engine::Session& session, RefId<MethodReference> index) const;
    Offset<FieldReference> Query(Engine::Session& session, RefId<FieldReference> index) const;
    Offset<Term> Query(Engine::Session& session, RefId<Term> index) const;

    IO::OffsetPool<MethodReference> const& MethodReferencesOffsets() const;
    IO::OffsetPool<FieldReference> const& FieldReferencesOffsets() const;
    IO::OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> const& TermsOffsets() const;

private:
    RegionData(
        IO::FileId fileId,
        IO::OffsetPool<MethodReference> methods,
        IO::OffsetPool<FieldReference> fields,
        IO::OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms
    );

    IO::FileId fileId;

    IO::OffsetPool<MethodReference> methods;
    IO::OffsetPool<FieldReference> fields;
    IO::OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms;
};

} // namespace Symlevel
