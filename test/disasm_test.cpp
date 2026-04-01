#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/disasm.h"

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
        10,
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
    ::std::stringstream ss;
    Disassembler rw(nullptr, methodCode, ss);
    rw.Interpret();
    EXPECT_EQ(ss.str(),
R"(10: movi.W64 IR1 0
20: movi.W64 IR2 1
30: movi.W64 IR3 0
35: jmp 26
38: add.W64 IR4 IR3 IR2
49: add.imm.W64 IR3 IR1 1
51: mov IR1 IR2
53: mov IR5 IR3
55: mov IR3 IR1
57: mov IR2 IR4
59: mov IR6 IR1
61: mov IR1 IR5
63: mov IR4 IR1
78: branchif.LT.W64 IR1 10 -43
80: mov IR1 IR3
82: ret.W64 IR1
)");
}

} // namespace Cbc
