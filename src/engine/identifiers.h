#pragma once

#include "engine/packed_identifier.h"
#include "symlevel/io/file_id.h"
#include "symlevel/offset.h"
#include <cstdint>

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> struct Identifier {
    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : ident(0, offs, fileId) {}

    Identifier(uint64_t raw) : ident(raw) {}

    Identifier(Identifier<T> const& another) : ident(another.ident) {}

    Symlevel::Offset<T> GetOffset() const { return ident.GetHigh(); }

    IO::FileId GetFileId() const { return ident.GetLow(); }

    bool operator==(const Identifier<T>& another) const { return ident == another.ident; }

    uint64_t GetHash() const
    {
        std::hash<Engine::PackedIdentifier> hasher;
        return hasher(ident);
    }

private:
    PackedIdentifier ident;
};

} // namespace Engine

template <typename T> struct std::hash<Engine::Identifier<T>> {
    uint64_t operator()(Engine::Identifier<T> const& ident) const { return ident.GetHash(); }
};
