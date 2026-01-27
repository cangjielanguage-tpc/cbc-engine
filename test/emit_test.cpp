#include <gtest/gtest.h>

#define UNIT_TEST_MODE 1

#include "cbc/emitter/emitter.h"
#include "interpreter/interpreter.h"

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

using namespace Cbc::Format;
// Stub entry-point
template <typename Handler = Interpretation::Interpreter>
void Entry(Handler handler, Interpretation::Interpreter::Context ctx, Decoder::ByteReader stream) {
    using namespace Decoder;
    NEXT;
}

Interpretation::Value::Primitive U32(uint32_t v) {
    return Interpretation::Value::Primitive{.u32 = v};
}

static Interpretation::Value::Primitive Interpret(
        Code code, Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2)
{
    Interpretation::Ectype ectype{};
    Interpretation::Interpreter interp {
        .ectype = &ectype,
        .frame = nullptr,
    };
    Interpretation::Interpreter::Context ctx {
        .handle = nullptr,
        .literals = code.literals,
    };
    Decoder::ByteReader s(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    ectype.Put(IReg::IR1, ir1);
    ectype.Put(IReg::IR2, ir2);

    Entry(interp, ctx, s);

    return ectype.GetPrimitive(IReg::IR1);
}

TEST(EmitTest, Simple_ArithB2rr) {
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(3, code.bytecodeSize);

    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB3xrrr) {
    GTEST_SKIP() << "Decoding/dispatching of b3xrrr not implemented yet";
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
    for (int i = 0; i < INT16_MAX; ++i) {
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
    for (int i = 0; i < INT16_MAX + 1; ++i) {
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
    for (int i = 0; i < -INT16_MIN - Format::ExtBrr::INSTRUCTION_SIZE; ++i) {
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
    for (int i = 0; i < -INT16_MIN - Format::ExtBrr::INSTRUCTION_SIZE + 1; ++i) {
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
