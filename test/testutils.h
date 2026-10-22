#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "engine/image/io/random_access_file.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include <memory>

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
        FATAL("Not enough memory");
    }

    void Free(void* p, size_t bytes, size_t alignment) override {}

    ~LimitedHeap() override {}
};

bool CheckForAssembler();
// Assembles a resource .asm from the test resources directory: by default the file is
// first copied under a temporary name into the test's auto-generated directory and
// assembled there, so the resources directory is never written to. Pass next_only=false
// to assemble in place instead (the fresh .cbc is then relocated out of the resources
// directory).
std::unique_ptr<IO::RandomAccessFile> OpenAsm(std::string file_name, bool next_only = true);
// Assembles an in-memory asm source: writes it under the test autogen directory,
// runs the assembler and opens the produced .cbc. `name` is the image name also
// used by the engine for method lookups; it may contain '/' to form subdirectories.
std::unique_ptr<IO::RandomAccessFile> OpenAsmText(const std::string& name, std::string_view asm_text);

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
