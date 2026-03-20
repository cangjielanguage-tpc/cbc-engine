#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/isa.h"
#include "cbc/rewriter.h"

#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class RewriterTest : public testing::Test {
    void SetUp() override
    {
        InitializeMockInterpreter();
        heap.Reset();
    }

    void TearDown() override {}
};

namespace Cbc {

struct Test;

using namespace Cbc::Format;

TEST_F(RewriterTest, Rewriter_Simple)
{
    // GTEST_SKIP() << "Isa12 bytecode changed";
    uint32_t isa12CodeSize = 4;
    uint8_t isa12Bytes[]   = {
        bits(opcode::Add32), //
        static_cast<uint8_t>(IReg::IR1 << 4) | IReg::IR2, // Add IR1, IR2
        bits(opcode::Ret), //
        static_cast<uint8_t>(Width::W32 << 4) | IReg::IR1 // Ret IR1
    };
    MethodCode methodCode = MethodCode::Mock(isa12Bytes, isa12CodeSize);

    Emitter::Emitter e;
    Rewriter rw(nullptr, methodCode, e);
    rw.Interpret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

} // namespace Cbc
