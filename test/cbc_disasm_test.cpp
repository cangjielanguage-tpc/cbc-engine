#include <gtest/gtest.h>
#include <sstream>

#include "cbc/formater_rt.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "engine/image/reader.h"
#include "interpreter/function_handle.h"
#include "testutils.h"
#include "engine/options.h"
#include "utils/ostream.h"

static LimitedHeap<16384> heap;

class CbcDisasmTest : public testing::Test {
    void SetUp() override
    {
        heap.Reset();
        Engine::InitEnvOptions();
    }

    void TearDown() override {}
};

static void CompareWith(std::string_view fileName, std::string const& expected)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);

    auto mainId = engine.FindMain(session, fileName);
    ASSERT_TRUE(mainId.has_value());

    auto def  = Decode::Read(session, mainId.value());
    auto code = Decode::Read(session, def.MethodCode().value());

    Resolution::Resolver resolver(session, mainId.value());

    Stream::StringBuffer stream;
    Cbc::Disasm(stream, code, &resolver);

    ASSERT_EQ(expected, stream.ToString());
}

TEST_ASM(CbcDisasmTest, VirtCall)
{
    std::string expected = "0: newobj IR1, @default:Foo\n"
                           "3: call.virtual @default:Foo.foo()Void (1, 1) [IR1]\n"
                           "7: ret.W64 IR1\n";
    CompareWith("cbc-virt-call.asm", expected);
}

// A loop renders an arrow for the backward branch: the head on the target
// line, the corner on the branch line and a vertical bar on the lines between.
TEST_ASM(CbcDisasmTest, LoopArrows)
{
    std::string expected =
        " 0: mov.W64 IR3, 1\n"
        " 3: mov.W64 IR1, 0\n"
        " 6: mov.W64 IR2, 7\n"
        " 9: add.W64 IR1, IR1, IR2 <────────────────┐\n"
        "12: sub.W64 IR2, IR2, IR3                  │\n"
        "15: bcc.W64 1, IR2, IRZ, -10 ──────────────┘\n"
        "19: ret.W64 IR1\n";
    CompareWith("simple.asm", expected);
}

// Forward branches must not render arrows.
TEST_ASM(CbcDisasmTest, ForwardBranchNoArrows)
{
    std::string expected =
        " 0: mov.W64 IR3, 0\n"
        " 3: bcc.W64 0, IR3, IRZ, 3\n"
        " 7: mov.W64 IR1, 1\n"
        "10: ret.W64 IR1\n";
    CompareWith("cbc-fwd-branch.asm", expected);
}

// Direct disasm of the InitOption Option opcodes via Cbc::RT::Log.
// These verify the ISA-RT opcode-table format strings round-trip:
//   INIT_NONE_GENERIC_OPT0 -> "init.none.g0 $1ir"
//   INIT_SOME_GENERIC_OPT0 -> "init.some.g0 $0ir $1ir"
// where $0 = option (src), $1 = underlyingTi.
static std::string RtLog(Cbc::RT::InitOption args)
{
    Stream::StringBuffer stream;
    Cbc::RT::Log(nullptr, stream, args);
    return stream.ToString();
}

TEST(CbcRtDisasmTest, InitNoneGeneric)
{
    // operands = { option, underlyingTi }; only $1 (underlyingTi) used.
    Cbc::RT::InitOption args {
        .opc          = Cbc::RT::Opcode::INIT_NONE_GENERIC_OPT0,
        .src          = Cbc::IReg::IRZ,
        .underlyingTi = Cbc::IReg::IR1,
    };
    EXPECT_EQ("init.none.g0 IR1\n", RtLog(args));
}

TEST(CbcRtDisasmTest, InitSomeGeneric)
{
    Cbc::RT::InitOption args {
        .opc          = Cbc::RT::Opcode::INIT_SOME_GENERIC_OPT0,
        .src          = Cbc::IReg::IR2,
        .underlyingTi = Cbc::IReg::IR1,
    };
    EXPECT_EQ("init.some.g0 IR2 IR1\n", RtLog(args));
}

// A backward BCC64I in the rewritten RT ISA renders an arrow, same as in the
// CBC ISA disasm.
TEST(CbcRtDisasmTest, LoopArrows)
{
    using Cbc::RT::Opcode;
    // clang-format off
    uint8_t bytecode[] = {
        static_cast<uint8_t>(Opcode::NOP),     // 0x000: nop
        static_cast<uint8_t>(Opcode::BCC64I),  // 0x001: bcc.64 ne IR2, IR1, -5
        0xB1, 0xFF,                            //   XImm12: cc = NE (1), offset = 0xFFB (-5)
        0x12,                                  //   RR: IR2, IR1
        static_cast<uint8_t>(Opcode::NOP),     // 0x005: nop
        static_cast<uint8_t>(Opcode::RET),     // 0x006: ret
    };
    // clang-format on
    Interpretation::Code code{sizeof(bytecode), bytecode, nullptr};

    Stream::StringBuffer stream;
    Cbc::RT::Log(code, stream);

    std::string expected =
        "0x000: nop <──────────────────────────────────┐\n"
        "0x001: bcc.64 ne IR2 IR1 -5 ──────────────────┘\n"
        "0x005: nop\n"
        "0x006: ret\n";
    EXPECT_EQ(expected, stream.ToString());
}
