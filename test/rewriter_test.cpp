#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/isa.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_rewriter.h"

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

#define TEST_REWRITER_SIMPLE(REG1, REG2, OP, NAME_OP)                                                                  \
    TEST_F(RewriterTest, RewriterSimple##NAME_OP)                                                                      \
    {                                                                                                                  \
        uint32_t isa12CodeSize = 4;                                                                                    \
        uint8_t isa12Bytes[]   = {                                                                                     \
            Opcode::NAME_OP##32,                                                                                       \
            (IReg::IR1 << 4) | IReg::IR2, /* Add IR1, IR2 */                                                           \
            Opcode::RegGroup,                                                                                          \
            (RegGroup::Ret32 << 4) | IReg::IR1 /* Ret IR1 */                                                           \
        };                                                                                                             \
        MethodCode methodCode = MethodCode::Mock(isa12Bytes, isa12CodeSize);                                           \
        RawDisasm(stderr, methodCode)->ParseAll();                                                                     \
        Emitter::Emitter e;                                                                                            \
        Rewriter(*MockResolver(), methodCode, e)->ParseAll();                                                          \
        auto code = e.Build(heap);                                                                                     \
        auto res  = Interpret(code, U32(REG1), U32(REG2));                                                             \
        EXPECT_EQ(res.u32, REG1 OP REG2);                                                                              \
    }

TEST_REWRITER_SIMPLE(1, 2, +, Add)
TEST_REWRITER_SIMPLE(1, 2, -, Sub)
TEST_REWRITER_SIMPLE(1, 2, *, Mul)
TEST_REWRITER_SIMPLE(1, 2, &, And)
TEST_REWRITER_SIMPLE(1, 2, |, Or)
TEST_REWRITER_SIMPLE(1, 2, ^, Xor)
TEST_REWRITER_SIMPLE(1, 2, /, UDiv)
TEST_REWRITER_SIMPLE(1, 2, %, URem)
TEST_REWRITER_SIMPLE(1, 2, >>, LSR)
TEST_REWRITER_SIMPLE(1, 2, >>, ASR)
TEST_REWRITER_SIMPLE(1, 2, <<, LSL)

} // namespace Cbc
