#pragma once

#include <functional>
#include <stdint.h>

#include "symlevel/io/file_id.h"
#include "symlevel/offset.h"

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> struct Identifier {
    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : raw(0)
    {
        packed.unused = 0;
        packed.offset = offs;
        packed.fileId = fileId;
    }

    Identifier(uint64_t raw) : raw(raw) {}

    Identifier(Identifier<T> const& another) : raw(another.raw) {}

    operator uint64_t() const { return raw; }

    Symlevel::Offset<T> GetOffset() const { return packed.offset; }

    IO::FileId GetFileId() const { return packed.fileId; }

    bool operator==(const Identifier<T>& another) const { return raw == another.raw; }

private:
    union {
        struct {
            uint64_t unused : 16;
            uint64_t offset : 24;
            uint64_t fileId : 24;
        } packed;

        uint64_t raw;
    };
};

} // namespace Engine

template <typename T> struct std::hash<Engine::Identifier<T>> {
    uint64_t operator()(Engine::Identifier<T> const& ident) const
    {
        std::hash<uint64_t> hasher;
        return hasher(ident);
    }
};
