#pragma once

#include <cstddef>
#include <utility>

namespace Memory {

struct Heap {
    static Heap& SharedHeap();

    virtual ~Heap()                                                                             = default;
    virtual void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))          = 0;
    virtual void Free(void* memory, size_t bytes, size_t alignment = alignof(std::max_align_t)) = 0;

    template <typename T, typename... Args> T* New(Args&&... args)
    {
        void* memory = this->Allocate(sizeof(T), alignof(T));
        return new (memory) T(std::forward<Args>(args)...);
    }
};

} // namespace Memory
