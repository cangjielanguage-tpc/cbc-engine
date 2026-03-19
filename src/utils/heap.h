#pragma once

#include <cstddef>

namespace Memory {

struct Heap {
    static Heap& SharedHeap();

    virtual ~Heap()                                                                             = default;
    virtual void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))          = 0;
    virtual void Free(void* memory, size_t bytes, size_t alignment = alignof(std::max_align_t)) = 0;
};

} // namespace Memory
