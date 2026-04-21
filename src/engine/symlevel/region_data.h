#pragma once

#include "io/offset_pool.h"
#include "references.h"
#include "terms.h"

namespace Symlevel {

class RegionData {
public:
    static RegionData Read(IO::FileId fileId, IO::RandomAccessFile& file, uint32_t offset);

    std::optional<MethodReference> queryMethod(Engine::Session& session, Index<MethodReference> index) const;
    std::optional<FieldReference> queryField(Engine::Session& session, Index<FieldReference> index) const;
    std::optional<Term> queryTerm(Engine::Session& session, Index<Term> index) const;

private:
    RegionData(IO::FileId fileId, IO::OffsetPool methods, IO::OffsetPool fields, IO::OffsetPool terms);

    IO::FileId fileId;

    IO::OffsetPool methods;
    IO::OffsetPool fields;
    IO::OffsetPool terms;
};

} // namespace Symlevel
