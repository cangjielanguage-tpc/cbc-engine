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

#define TEST_REWRITER_SIMPLE(REG1,REG2,OP,NAME) \
    TEST_F(RewriterTest, Rewriter_Simple##NAME) \
    { \
        uint32_t isa12CodeSize = 4; \
        uint8_t isa12Bytes[]   = { \
            bits(opcode::Add32), \
            (IReg::IR1 << 4) | IReg::IR2, /* Add IR1, IR2 */ \
            bits(opcode::Ret32), \
            IReg::IR1 /* Ret IR1 */\
        }; \
        MethodCode methodCode = MethodCode::Mock(isa12Bytes, isa12CodeSize); \
        Emitter::Emitter e; \
        Rewriter rw(nullptr, methodCode, e); \
        rw.Interpret(); \
        auto code = e.Build(heap); \
        EXPECT_EQ(4, code.bytecodeSize); \
        auto res = Interpret(code, U32(REG1), U32(REG2)); \
        EXPECT_EQ(res.u32, REG1 + REG2); \
    }

TEST_REWRITER_SIMPLE(1,2,+,plus)
TEST_REWRITER_SIMPLE(1,2,-,minus)
TEST_REWRITER_SIMPLE(1,2,*,mul)
TEST_REWRITER_SIMPLE(1,2,&,and)
TEST_REWRITER_SIMPLE(1,2,|,or)
TEST_REWRITER_SIMPLE(1,2,^,xor)
TEST_REWRITER_SIMPLE(1,2,/,divSigned)
TEST_REWRITER_SIMPLE(1,2,%,remSigned)
TEST_REWRITER_SIMPLE(1,2,/,divUnsigned)
TEST_REWRITER_SIMPLE(1,2,%,remUnsigned)
TEST_REWRITER_SIMPLE(1,2,>>,lsr)
TEST_REWRITER_SIMPLE(1,2,>>,asr)
TEST_REWRITER_SIMPLE(1,2,<<,lsl)

} // namespace Cbc
