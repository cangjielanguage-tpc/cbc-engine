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
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    Identifier(Symlevel::Offset<T> offs, IO::FileId fileId) : offs(offs), fileId(fileId) {}

    Identifier(Packed packed)
        : Identifier(
              Symlevel::Offset<T>(packed & Symlevel::Offset<T>::MASK),
              IO::FileId((packed >> Symlevel::Offset<T>::BIT_SIZE) & IO::FileId::MASK)
          )
    {}

    Symlevel::Offset<T> GetOffset() const { return offs; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const Identifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = offs;
        uint64_t high = fileId;
        return low | (high << Symlevel::Offset<T>::BIT_SIZE);
    }

private:
    Symlevel::Offset<T> offs;
    IO::FileId fileId;
};

template <typename T> struct RefIdentifier {
    using Packed = uint64_t;

    struct Hasher {
        inline size_t operator()(Packed const& packed) const
        {
            std::hash<uint64_t> hash;
            return hash(Bits::Raw64(packed));
        }
    };

    RefIdentifier(Symlevel::RefId<T> index, IO::FileId fileId) : index(index), fileId(fileId) {}

    RefIdentifier(Packed packed)
        : RefIdentifier(
              Symlevel::RefId<T>(packed & Symlevel::RefId<T>::MASK),
              IO::FileId((packed >> Symlevel::RefId<T>::BIT_SIZE) & IO::FileId::MASK)
          )
    {}

    Symlevel::RefId<T> GetIndex() const { return index; }

    IO::FileId GetFileId() const { return fileId; }

    bool operator==(const RefIdentifier& another) const { return Pack() == another.Pack(); }

    inline Packed Pack() const
    {
        uint64_t low  = index;
        uint64_t high = fileId;
        return low | (high << Symlevel::RefId<T>::BIT_SIZE);
    }

private:
    Symlevel::RefId<T> index;
    IO::FileId fileId;
};

} // namespace Engine
