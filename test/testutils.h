#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "api/resolver.h"
#include "engine/symlevel/io/random_access_file.h"
#include "utils/heap.h"
#include <memory>
#include <stdexcept>

template <size_t limit> class LimitedHeap : public Memory::Heap {
public:
    void Reset() { cursor = (uintptr_t)memory; }

    uint8_t memory[limit];
    uintptr_t cursor { (uintptr_t)memory };
    uintptr_t end { cursor + limit };

    void* Allocate(std::size_t bytes, std::size_t alignment) override
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

    void Free(void* p, size_t bytes, size_t alignment) override {}

    ~LimitedHeap() override {}
};

std::unique_ptr<API::Resolver> MockResolver();

bool CheckForAssembler();
std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name);

#define TEST_ASM(test_suite_name, test_name)                                                                           \
    static void test_suite_name##_##test_name();                                                                       \
    GTEST_TEST_F(test_suite_name, test_name)                                                                           \
    {                                                                                                                  \
        if (!CheckForAssembler()) {                                                                                    \
            GTEST_SKIP() << "Assembler is not present";                                                                \
        }                                                                                                              \
        test_suite_name##_##test_name();                                                                               \
    }                                                                                                                  \
    static void test_suite_name##_##test_name()

#endif
