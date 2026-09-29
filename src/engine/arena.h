#pragma once

#include "utils/heap.h"
#include "utils/span.h"
#include <cstddef>
#include <cstdint>

namespace Engine {

/// Thread-unsafe growable memory arena.
class Arena : public Memory::Heap {
public:
    static constexpr size_t CHUNK_SIZE     = 2048;
    static constexpr size_t MAX_ALLOC_SIZE = 256;

    Arena() : cursor(0), end(0), chunks(nullptr) {}

    Arena(Arena const& another) = delete;
    ~Arena();

    void* Allocate(size_t bytes, size_t alignment) override;
    void Free(void* memory, size_t bytes, size_t alignment) override;

    template <typename T> Utils::Span<T> Copy(Utils::Span<T> span)
    {
        static_assert(std::is_trivially_destructible_v<T>, "T must not have a destructor (or it must be empty)");

        auto size = span.Size();
        T* memory = reinterpret_cast<T*>(Allocate(sizeof(T), alignof(T)));
        for (size_t i = 0; i < size; i++) {
            new (memory + i) T(span[i]);
        }
        return Utils::Span<T>(memory, size);
    }

private:
    void* DoAllocateSlow(size_t bytes);

    struct Chunk {
        static_assert(sizeof(Chunk*) <= alignof(std::max_align_t));

        // To ensure that `memory` field is properly aligned.
        union {
            Chunk* next;
        };

        union {
            max_align_t _pad;
            char memory[];
        };
    };

    uintptr_t cursor;
    uintptr_t end;

    Chunk* chunks;
};

} // namespace Engine
