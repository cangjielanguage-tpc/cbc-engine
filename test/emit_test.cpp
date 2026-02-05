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

TEST(EmitTest, Simple_Mov) {
    Emitter e;
    e.MovImm(Width::W64, IReg::IR2, 0x7);
    e.Mov(IReg::IR1, IReg::IR2);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(5, code.bytecodeSize);

    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u64, 0x7);
}

TEST(EmitTest, Simple_FMovI32) {
    Emitter e;
    e.FMovI32(FReg::FR0, 0.5);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(7, code.bytecodeSize);

    auto res = InterpretFPRes(code, U32(0), U32(0));
    EXPECT_EQ(res.f32, 0.5);
}

TEST(EmitTest, Simple_FMovI64) {
    Emitter e;
    e.FMovI64(FReg::FR0, 0.25);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(11, code.bytecodeSize);

    auto res = InterpretFPRes(code, U32(0), U32(0));
    EXPECT_EQ(res.f64, 0.25);
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

TEST(EmitTest, Simple_ArithB4xi12rr) {
    Emitter e;
    e.AddI(Width::W32, IReg::IR1, IReg::IR2, 0xff);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 0xff00); // through literal
    e.SubI(Width::W32, IReg::IR1, IReg::IR1, 0xfff);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(13, code.bytecodeSize);

    auto res = Interpret(code, U32(0), U32(1));
    EXPECT_EQ(res.u32, 0xf001);
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

TEST(EmitTest, Simple_Bcc_Loop) {
    Emitter e;
    auto loop = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.Bcc(CC::LT, Width::W32, IReg::IR1, IReg::IR2, loop);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(0), U32(100));
    EXPECT_EQ(res.u32, 100);
}

TEST(EmitTest, Simple_Jmp) {
    Emitter e;
    auto l1 = e.NewLabel();
    auto l2 = e.NewLabel();
    auto l3 = e.NewLabel();
    e.Jmp(l2);
    e.MovImm(Width::W32, IReg::IR1, 1);
    e.Bind(l1);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 10);
    e.Jmp(l3);
    e.MovImm(Width::W32, IReg::IR1, 2);
    e.Bind(l2);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 20);
    e.Jmp(l1);
    e.MovImm(Width::W32, IReg::IR1, 3);
    e.Bind(l3);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 30);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(100), U32(0));
    EXPECT_EQ(res.u32, 160);
}

TEST(EmitTest, Simple_BccImm) {
    Emitter e;
    auto loop = e.NewLabel();
    auto fwd = e.NewLabel();
    auto end = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.BccImm(CC::NE, Width::W32, IReg::IR1, 0xffff, fwd); // lit value, lit offset
    for (int i = 0; i < INT16_MAX / 8; ++i) {
        e.Ret();
    }
    e.Bind(fwd);
    e.BccImm(CC::LT, Width::W32, IReg::IR1, 100, loop); // lit neg offset
    e.BccImm(CC::LT, Width::W32, IReg::IR1, -0xffff, end); // lit neg value, false res
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.BccImm(CC::GE, Width::W32, IReg::IR1, -5, end); // neg value
    e.Ret();
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 2);
    e.Bind(end);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ(res.u32, 101);
}

} // namespace Emitter
} // namespace Cbc
