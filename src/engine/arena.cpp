#include "utils/assertion.h"
#include <cstddef>
#include <cstdlib>

#include "arena.h"

namespace Engine {

static uintptr_t Align(uintptr_t value)
{
    auto alignment = alignof(std::max_align_t);
    auto rem       = value % alignment;
    auto result    = rem == 0 ? value : value + (alignment - rem);
    ASSERT((result % alignment) == 0);
    return result;
}

void* Arena::DoAllocateSlow(size_t bytes)
{
    static_assert(CHUNK_SIZE > sizeof(Chunk));
    static_assert(MAX_ALLOC_SIZE < CHUNK_SIZE);

    if (bytes > MAX_ALLOC_SIZE) {
        void* mem = malloc(bytes + sizeof(Chunk));
        if (mem == nullptr) {
            throw std::bad_alloc();
        }
        Chunk* newChunk = reinterpret_cast<Chunk*>(mem);
        newChunk->next  = chunks;
        this->chunks    = newChunk;

        auto memoryStart = reinterpret_cast<uintptr_t>(newChunk->memory);
        ASSERT(memoryStart == Align(memoryStart));

        return newChunk->memory;
    }

    void* mem = reinterpret_cast<Chunk*>(malloc(CHUNK_SIZE));
    if (mem == nullptr) {
        throw std::bad_alloc();
    }
    Chunk* newChunk = reinterpret_cast<Chunk*>(mem);
    uintptr_t end   = reinterpret_cast<uintptr_t>(mem) + CHUNK_SIZE;

    newChunk->next = this->chunks;
    this->chunks   = newChunk;
    this->end      = end;

    auto cursor = reinterpret_cast<uintptr_t>(newChunk->memory);
    ASSERT(cursor == Align(cursor));
    auto newCursor = Align(cursor + bytes);
    this->cursor   = newCursor;

    return reinterpret_cast<void*>(cursor);
}

void* Arena::do_allocate(size_t bytes, size_t alignment)
{
    ASSERT(this->cursor == Align(this->cursor));

    auto newCursor = Align(this->cursor + bytes);
    if (newCursor >= this->end || bytes > MAX_ALLOC_SIZE) {
        return DoAllocateSlow(bytes);
    }

    auto result  = reinterpret_cast<void*>(cursor);
    this->cursor = newCursor;
    return result;
}

Arena::~Arena()
{
    auto chunk = chunks;
    while (chunk) {
        auto next = chunk->next;
        free(chunk);
        chunk = next;
    }
}

} // namespace Engine
