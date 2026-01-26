#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"

class Heap : public std::pmr::memory_resource {
public:
    void Reset() {
        cursor = (uintptr_t) memory;
    }

    static constexpr size_t MEMORY_LIMIT = 4096;

    uint8_t memory[MEMORY_LIMIT];
    uintptr_t cursor{(uintptr_t) memory};
    uintptr_t end{cursor + MEMORY_LIMIT};

    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        auto result = cursor;
        auto rem = result % alignment;
        result = rem == 0 ? result : result + (alignment - rem);
        auto newCursor = result + bytes;
        if (newCursor < end) {
            cursor = newCursor;
            return (void*) result;
        }
        throw std::runtime_error("Not enough memory");
    }

    void do_deallocate(void *p, size_t bytes, size_t alignment) override {}

    bool do_is_equal(const memory_resource &other) const noexcept override {
        return this == &other;
    }
};

static Heap heap;

namespace Cbc {
namespace Emitter {

class EmitTest : public testing::Test {
    void SetUp() override {
        heap.Reset();
    }

    void TearDown() override {

    }
};

using namespace Cbc::Format;

TEST(EmitTest, Simple_ArithB2rr) {
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR3);
    auto code = e.Build(heap);
    EXPECT_EQ(2, code.bytecodeSize);
}

TEST(EmitTest, Simple_ArithB3xrrr) {
    Emitter e;
    e.Add(Width::W32, IReg::IR3, IReg::IR1, IReg::IR3);
    auto code = e.Build(heap);
    EXPECT_EQ(3, code.bytecodeSize);
}

TEST(EmitTest, Literals_none) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bind(label);
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR1, label);

    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);
    EXPECT_EQ(0, code.literals->size);
}

} // namespace Emitter
} // namespace Cbc
