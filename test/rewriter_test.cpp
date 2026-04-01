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

#define TEST_REWRITER_SIMPLE(REG1, REG2, OP, NAME_OP)                                                                  \
    TEST_F(RewriterTest, RewriterSimple##NAME_OP)                                                                      \
    {                                                                                                                  \
        uint32_t isa12CodeSize = 4;                                                                                    \
        uint8_t isa12Bytes[]   = {                                                                                     \
            Opc(InputOpcode::NAME_OP##32),                                                                           \
            (IReg::IR1 << 4) | IReg::IR2, /* Add IR1, IR2 */                                                         \
            Opc(InputOpcode::Ret32),                                                                                 \
            IReg::IR1 /* Ret IR1 */                                                                                  \
        };                                                                                                             \
        MethodCode methodCode = MethodCode::Mock(isa12Bytes, isa12CodeSize);                                           \
        Emitter::Emitter e;                                                                                            \
        Rewriter rw(nullptr, methodCode, e);                                                                           \
        rw.Interpret();                                                                                                \
        auto code = e.Build(heap);                                                                                     \
        auto res = Interpret(code, U32(REG1), U32(REG2));                                                              \
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

TEST_F(RewriterTest, RewriterSimpleFibonacci)
{
    uint32_t isa12CodeSize = 82;
    uint8_t isa12Bytes[]   = {
        Opc(InputOpcode::Mov64i),
        (IReg::IR1 << 4) | 0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        Opc(InputOpcode::Mov64i),
        (IReg::IR2 << 4) | 0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        Opc(InputOpcode::Mov64i),
        (IReg::IR3 << 4) | 0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        Opc(InputOpcode::Jump32),
        26,
        0,
        0,
        0,
        Opc(InputOpcode::Binary64),
        (Opc(InputCommonOpc::Add) << 4) | IReg::IR4,
        (IReg::IR3 << 4) | IReg::IR2,
        Opc(InputOpcode::BinaryImm64),
        (Opc(InputCommonOpc::Add) << 4) | IReg::IR3,
        (IReg::IR1 << 4) | 0,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        Opc(InputOpcode::Mov64),
        (IReg::IR1 << 4) | IReg::IR2,
        Opc(InputOpcode::Mov64),
        (IReg::IR5 << 4) | IReg::IR3,
        Opc(InputOpcode::Mov64),
        (IReg::IR3 << 4) | IReg::IR1,
        Opc(InputOpcode::Mov64),
        (IReg::IR2 << 4) | IReg::IR4,
        Opc(InputOpcode::Mov64),
        (IReg::IR6 << 4) | IReg::IR1,
        Opc(InputOpcode::Mov64),
        (IReg::IR1 << 4) | IReg::IR5,
        Opc(InputOpcode::Mov64),
        (IReg::IR4 << 4) | IReg::IR1,
        Opc(InputOpcode::BccImm),
        (Opc(InputCcOpc::LT) << 4) | Width::W64,
        (0 << 4) | IReg::IR1,
        7,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        (uint8_t)-43,
        (uint8_t)-1,
        (uint8_t)-1,
        (uint8_t)-1,
        Opc(InputOpcode::Mov64),
        (IReg::IR1 << 4) | IReg::IR3,
        Opc(InputOpcode::Ret64),
        (0 << 4) | IReg::IR1
    };
    MethodCode methodCode = MethodCode::Mock(isa12Bytes, isa12CodeSize);
    Emitter::Emitter e;
    Rewriter rw(nullptr, methodCode, e);
    rw.Interpret();
    auto code = e.Build(heap);
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u64, 13);
}

} // namespace Cbc
