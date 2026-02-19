#ifndef TESTUTILS_H
#define TESTUTILS_H

#include <memory_resource>
#include <stdexcept>

template <size_t limit> class LimitedHeap : public std::pmr::memory_resource {
public:
    void Reset() { cursor = (uintptr_t)memory; }

    uint8_t memory[limit];
    uintptr_t cursor { (uintptr_t)memory };
    uintptr_t end { cursor + limit };

    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        auto result    = cursor;
        auto rem       = result % alignment;
        result         = rem == 0 ? result : result + (alignment - rem);
        auto newCursor = result + bytes;
        if (newCursor < end) {
            cursor = newCursor;
            return (void*)result;
        }
        throw std::runtime_error("Not enough memory");
    }

    void do_deallocate(void* p, size_t bytes, size_t alignment) override {}

    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

#endif
