#include <gtest/gtest.h>

#define UNIT_TEST_MODE 1

#include "cbc/emitter/emitter.h"
#include "cbc/isa_rt.h"

#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class EmitTest : public testing::Test {
    void SetUp() override {
        heap.Reset();
    }

    void TearDown() override {

    }
};

namespace Cbc {
namespace Emitter {

struct Test;

static constexpr int MAX_I12 = Cbc::RT::LIT_TABLE_SIZE / 2 - 1;
static constexpr int MIN_I12 = -Cbc::RT::LIT_TABLE_SIZE / 2;

using namespace Cbc::Format;

TEST(EmitTest, Simple_ArithB2rr) {
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB3xrrr) {
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR2, IReg::IR1);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Literals_None) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR1, label);
    for (int i = 0; i < MAX_I12; ++i) {
        e.Ret();
    }
    e.Bind(label);

    LimitedHeap<INT16_MAX + 400> h;
    auto code = e.Build(h);
    EXPECT_EQ(code.literals->size(), 0);
}

TEST(EmitTest, Literals_Label) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR1, label);
    for (int i = 0; i < MAX_I12 + 1; ++i) {
        e.Ret();
    }
    e.Bind(label);

    LimitedHeap<INT16_MAX + 400> h;
    auto code = e.Build(h);
    EXPECT_EQ(code.literals->size(), 1);
}

TEST(EmitTest, Literals_NoneBackEdge) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bind(label);
    for (int i = 0; i < -MIN_I12 - Cbc::RT::B4xi12rr::SIZE; ++i) {
        e.Ret();
    }
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR1, label);

    LimitedHeap<INT16_MAX + 400> h;
    auto code = e.Build(h);
    EXPECT_EQ(code.literals->size(), 0);
}

TEST(EmitTest, Literals_LabelBackEdge) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bind(label);
    for (int i = 0; i < -INT16_MIN - Cbc::RT::B4xi12rr::SIZE + 1; ++i) {
        e.Ret();
    }
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR1, label);

    LimitedHeap<INT16_MAX + 400> h;
    auto code = e.Build(h);
    EXPECT_EQ(code.literals->size(), 1);
}

TEST(EmitTest, Simple_Bcc) {
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, label);
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Bind(label);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(2), U32(2));
    EXPECT_EQ(res.u32, 2);

    auto res2 = Interpret(code, U32(2), U32(1));
    EXPECT_EQ(res2.u32, 3);
}

} // namespace Emitter
} // namespace Cbc
