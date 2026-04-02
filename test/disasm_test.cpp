#include <gtest/gtest.h>

#include "cbc/disasm.h"
#include "cbc/emitter/emitter.h"

#include "cbc/isa.h"
#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class DisasmTest : public testing::Test {
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

TEST_F(DisasmTest, Fibonacci)
{
    uint32_t isa12CodeSize = 7;
    uint8_t isa12Bytes[]   = { Opc(InputOpcode::Mov64),      (IReg::IR3 << 4) | IReg::IR2,
                               Opc(InputOpcode::Binary64),   (Opc(InputCommonOpc::Add) << 4) | IReg::IR1,
                               (IReg::IR3 << 4) | IReg::IR2, Opc(InputOpcode::Ret64),
                               (0 << 4) | IReg::IR1 };
    MethodCode methodCode  = MethodCode::Mock(isa12Bytes, isa12CodeSize);
    Emitter::Emitter e;
    std::stringstream ss;
    Disassembler rw(nullptr, methodCode, ss);
    rw.Interpret();
    EXPECT_EQ(
        ss.str(),
        R"(2: mov IR3 IR2
5: add.W64 IR1 IR3 IR2
7: ret.W64 IR1

)"
    );
}

} // namespace Cbc
