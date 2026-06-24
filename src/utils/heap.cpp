#include "heap.h"
#include "utils/assertion.h"
#include <cstdlib>

namespace Memory {

struct OSHeap : public Heap {
    ~OSHeap() {}

    void* Allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))
    {
        auto result = malloc(bytes);
        if (!result) {
            FATAL("Out of memory");
        }
        return result;
    }

    void Free(void* memory, size_t bytes, size_t alignment = alignof(std::max_align_t)) { free(memory); };
};

static OSHeap g_OSHeap;

Heap& Heap::SharedHeap() { return g_OSHeap; }

} // namespace Memory
