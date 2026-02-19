#pragma once

#include "stdint.h"
#include "symlevel/io/file_id.h"
#include "symlevel/offset.h"
#include <functional>

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> struct Identifier {
    static constexpr uint64_t OFFSET_MASK = (1lu << 24) - 1;
    static constexpr uint64_t FILEID_MASK = ((1lu << 48) - 1) ^ OFFSET_MASK;

    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : value(offs.value | (fileId.id << 24)) {}

    // Identifier(Identifier&& another) = default;
    // Identifier(Identifier const& another) = default;

    operator uint64_t() const { return value; }

    Symlevel::Offset<T> GetOffset() const { return value & OFFSET_MASK; }

    IO::FileId GetFileId() const { return (value & FILEID_MASK) >> 24; }

private:
    uint64_t value;
};

class MethodDefIdentifier {
public:
    operator uint64_t() const { return value; }

private:
    uint64_t value;
};

using TypeDefIdentifier = void*;

} // namespace Engine

template <typename T> struct std::hash<Engine::Identifier<T>> {
    uint64_t operator()(Engine::Identifier<T> const& ident) const
    {
        std::hash<uint64_t> hasher;
        return hasher(ident);
    }
};
