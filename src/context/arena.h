#pragma once

#include <memory_resource>

namespace Contexts {

/// Thread-unsafe growable memory arena.
class Arena : public std::pmr::memory_resource {
public:
    static constexpr size_t CHUNK_SIZE = 2048;
    static constexpr size_t MAX_ALLOC_SIZE = 256;

    Arena() : cursor(0), end(0), chunks(nullptr) {}
    ~Arena();

    void* do_allocate(size_t bytes, size_t alignment) override;
    void do_deallocate(void *p, size_t bytes, size_t alignment) override { return; }
    bool do_is_equal(const memory_resource &other) const noexcept override { return false; }

private:
    void* DoAllocateSlow(size_t bytes);

    struct Chunk {
        static_assert(sizeof(Chunk*) <= alignof(std::max_align_t));

        // To ensure that `memory` field is properly aligned.
        union {
            Chunk *next;
            char _pad[alignof(std::max_align_t)];
        };
        char memory[];
    };


    uintptr_t cursor;
    uintptr_t end;

    Chunk *chunks;
};

} // namespace Contexts
