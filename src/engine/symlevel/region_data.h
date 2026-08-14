#pragma once

#include "engine/symlevel/io/random_access_file.h"
#include "io/offset_pool.h"
#include "references.h"
#include <cstdint>

namespace Symlevel {

struct RegionData {
    static constexpr uint32_t FIRST_NON_PRIMITIVE_TERM_ID = 20;

    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    RegionData(
        IO::OffsetPool<MethodReference> methods,
        IO::OffsetPool<FieldReference> fields,
        IO::OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms
    );

    IO::OffsetPool<MethodReference> methods;
    IO::OffsetPool<FieldReference> fields;
    IO::OffsetPool<Term, FIRST_NON_PRIMITIVE_TERM_ID> terms;

    template <typename T> IO::ErasedOffsetPool ErasedPool() const;

    template <> IO::ErasedOffsetPool ErasedPool<Term>() const { return terms.Erased(); }

    template <> IO::ErasedOffsetPool ErasedPool<MethodReference>() const { return methods.Erased(); }

    template <> IO::ErasedOffsetPool ErasedPool<FieldReference>() const { return fields.Erased(); }
};

} // namespace Symlevel
