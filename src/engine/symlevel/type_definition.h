#pragma once

#include <vector>
#include "io/stream_file_reader.h"
#include "offset.h"
#include "string.h"
#include "type_kind.h"
#include "io/file_id.h"
#include "engine/engine.h"


namespace Symlevel {

class TypeDefinition {
public:
    static TypeDefinition Parse(IO::FileId fileId, IO::StreamFileReader& reader);

    inline const Offset<String> Name() const { return name; }

    inline uint32_t GetIdx() const { return  idx; }

    inline TypeKind GetTypeKind() const { return typeKind; }

    inline std::vector<uint32_t> GetSupers() const { return supers; }

private:

    TypeDefinition(IO::FileId fileId, Offset<String> name, uint32_t idx, TypeKind typeKind, std::vector<uint32_t> supers):
        fileId(fileId),
        name(name),
        idx(idx),
        typeKind(typeKind),
        supers(std::move(supers))
    {}

    const IO::FileId fileId;

    const Offset<String> name;
    const uint32_t idx;
    const TypeKind typeKind;
    const std::vector<uint32_t> supers;
};

} // namespace Symlevel
