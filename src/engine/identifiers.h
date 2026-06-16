#pragma once

#include "engine/symlevel/index.h"
#include "symlevel/io/file_id.h"
#include "symlevel/offset.h"
#include "utils/reinterpretation.h"
#include <cstdint>
#include <cstring>
#include <functional>

namespace Engine {
/// An opaque handle to symlevel definitions.

template <typename T> struct Identifier {
    struct Packed {
        uint64_t const unused : 8;
        uint64_t const offs : Symlevel::Offset<T>::BIT_SIZE;
        uint64_t const fileId : IO::FileId::BIT_SIZE;

        inline bool operator==(const Packed& another) const { return Bits::Raw64(*this) == Bits::Raw64(another); }
    };

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : offs(offs), fileId(fileId) {}

    Identifier(Packed const& packed) : Identifier(Symlevel::Offset<T>(packed.offs), IO::FileId(packed.fileId)) {}

    Symlevel::Offset<T> GetOffset() const { return offs; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const Identifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const { return { 0, offs, static_cast<uint32_t>(fileId) }; }

private:
    Symlevel::Offset<T> offs;
    IO::FileId fileId;
};

template <typename T> struct RefIdentifier {
    struct Packed {
        uint64_t const unused : 12;
        uint64_t const region : 8;
        uint64_t const id : 16;
        uint64_t const fileId : IO::FileId::BIT_SIZE;

        bool operator==(Packed const& another) const { return Bits::Raw64(*this) == Bits::Raw64(another); }
    };

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    RefIdentifier(Symlevel::RefId<T> index, IO::FileId fileId) : index(index), fileId(fileId) {}

    RefIdentifier(Packed const& packed)
        : RefIdentifier(Symlevel::RefId<T>(packed.region, packed.id), IO::FileId(packed.fileId))
    {}

    Symlevel::RefId<T> GetIndex() const { return index; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const RefIdentifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const { return { 0, index.GetRegion(), index.GetIndex(), static_cast<uint32_t>(fileId) }; }

private:
    Symlevel::RefId<T> index;
    IO::FileId fileId;
};

} // namespace Engine
