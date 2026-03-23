#pragma once

#include "utils/heap.h"
#include <cstdint>

namespace Engine {

/// Thread-unsafe growable memory arena.
class Arena : public Memory::Heap {
public:
    static constexpr size_t CHUNK_SIZE     = 2048;
    static constexpr size_t MAX_ALLOC_SIZE = 256;

    Arena() : cursor(0), end(0), chunks(nullptr) {}

    ~Arena();
    void* Allocate(size_t bytes, size_t alignment) override;
    void Free(void* memory, size_t bytes, size_t alignment) override;

private:
    void* DoAllocateSlow(size_t bytes);

    struct Chunk {
        static_assert(sizeof(Chunk*) <= alignof(std::max_align_t));

        // To ensure that `memory` field is properly aligned.
        union {
            Chunk* next;
            char _pad[alignof(std::max_align_t)];
        };

        char memory[];
    };

    uintptr_t cursor;
    uintptr_t end;

    Chunk* chunks;
};

} // namespace Engine
