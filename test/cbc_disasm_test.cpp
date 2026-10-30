#include <gtest/gtest.h>
#include <sstream>

#include "cbc/formater_rt.h"
#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "engine/image/reader.h"
#include "interpreter/function_handle.h"

#include "engine/options.h"
#include "testutils.h"
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

TEST_ASM(CbcDisasmTest, Convert)
{
    std::string expected = " 0: mov.W64 IR1, 1\n"
                           " 3: convert I64, I32, IR1, IR1\n"
                           " 6: convert U32, U64, IR1, IR1\n"
                           " 9: load.ti.g IR1, @0\n";
    CompareWith("convert.asm", expected);
}

static std::string RawIsaDisasm(uint8_t* start, uint8_t* end)
{
    Stream::StringBuffer stream;
    Cbc::RawDisasm(stream, start, end);
    return stream.ToString();
}

TEST_F(CbcDisasmTest, ConvertRaw)
{
    uint8_t code[] = {
        // input ISA encodes a cast as (to << 4) | from type codes, i.e. I64 -> I32
        static_cast<uint8_t>(Cbc::Opcode::Convert),
        static_cast<uint8_t>((0x4 << 4) | 0x6),
        static_cast<uint8_t>(static_cast<uint8_t>(Cbc::IReg::IR2) | (static_cast<uint8_t>(Cbc::IReg::IR1) << 4)),
        static_cast<uint8_t>(Cbc::Opcode::CheckedConvert),
        static_cast<uint8_t>((0x4 << 4) | 0xa), // F64 -> I32
        static_cast<uint8_t>((static_cast<uint8_t>(Cbc::IReg::IR2) << 4) | static_cast<uint8_t>(Cbc::FReg::FR0)),
    };
    EXPECT_EQ(
        "0: convert I32, I64, IR1, IR2\n"
        "3: checked.convert I32, F64, IR2, FR0\n",
        RawIsaDisasm(code, code + sizeof(code))
    );
}

TEST_F(CbcDisasmTest, ConvertRawUnknownPair)
{
    uint8_t code[] = {
        static_cast<uint8_t>(Cbc::Opcode::Convert),
        static_cast<uint8_t>((0x0 << 4) | 0x0), // I8 -> I8, not in the cast table
        static_cast<uint8_t>(static_cast<uint8_t>(Cbc::IReg::IR2) | (static_cast<uint8_t>(Cbc::IReg::IR1) << 4)),
    };
    EXPECT_EQ("0: convert I8, I8, IR1, IR2\n", RawIsaDisasm(code, code + sizeof(code)));
}

TEST_F(CbcDisasmTest, CheckedConvertRawUnsupportedPair)
{
    uint8_t code[] = {
        static_cast<uint8_t>(Cbc::Opcode::CheckedConvert),
        static_cast<uint8_t>((0x6 << 4) | 0x4), // I32 -> I64, widening is not checked
        static_cast<uint8_t>(static_cast<uint8_t>(Cbc::IReg::IR2) | (static_cast<uint8_t>(Cbc::IReg::IR1) << 4)),
    };
    EXPECT_EQ("0: checked.convert I64, I32, IR1, IR2\n", RawIsaDisasm(code, code + sizeof(code)));
}

template <typename T> static std::string RtLog(T args)
{
    Stream::StringBuffer stream;
    Cbc::RT::Log(nullptr, stream, args);
    return stream.ToString();
}

TEST(CbcFormatTest, CastOpLayout)
{
    Cbc::Format::CastOp op = Cbc::Format::CastOp::I32_TO_I64;
    EXPECT_EQ(op.Raw(), static_cast<uint8_t>(Cbc::Format::CastOp::I32_TO_I64));
    EXPECT_EQ(op.FromTypeStr(), "I32");
    EXPECT_EQ(op.ToTypeStr(), "I64");
    ASSERT_TRUE(Cbc::Format::CastOp::FromPair(0x4, 0x6).has_value());
    EXPECT_EQ(Cbc::Format::CastOp::FromPair(0x4, 0x6)->Raw(), op.Raw());
    EXPECT_FALSE(Cbc::Format::CastOp::FromPair(0x0, 0x0).has_value());
    EXPECT_FALSE(Cbc::Format::CastOp::FromPair(0x4, 0x5).has_value()); // I32 -> U32 is not unchecked

    // Every op is reachable from its own (from, to) pair, and the enum is dense.
    for (uint8_t i = 0; i < Cbc::Format::CastOp::COUNT; ++i) {
        auto entry = Cbc::Format::CastOp(static_cast<Cbc::Format::CastOp::Value>(i));
        EXPECT_EQ(entry.Raw(), i);
        ASSERT_TRUE(Cbc::Format::CastOp::FromPair(entry.FromType(), entry.ToType()).has_value());
        EXPECT_EQ(Cbc::Format::CastOp::FromPair(entry.FromType(), entry.ToType())->Raw(), i);
    }
}

TEST(CbcFormatTest, CheckedCastOpLayout)
{
    Cbc::Format::CheckedCastOp op = Cbc::Format::CheckedCastOp::I64_TO_I32;
    EXPECT_EQ(op.Raw(), static_cast<uint8_t>(Cbc::Format::CheckedCastOp::I64_TO_I32));
    EXPECT_EQ(op.FromTypeStr(), "I64");
    EXPECT_EQ(op.ToTypeStr(), "I32");
    ASSERT_TRUE(Cbc::Format::CheckedCastOp::FromPair(0x6, 0x4).has_value());
    EXPECT_EQ(Cbc::Format::CheckedCastOp::FromPair(0x6, 0x4)->Raw(), op.Raw());
    EXPECT_FALSE(Cbc::Format::CheckedCastOp::FromPair(0x4, 0x6).has_value()); // I32 -> I64 is not checked

    for (uint8_t i = 0; i < Cbc::Format::CheckedCastOp::COUNT; ++i) {
        auto entry = Cbc::Format::CheckedCastOp(static_cast<Cbc::Format::CheckedCastOp::Value>(i));
        EXPECT_EQ(entry.Raw(), i);
        ASSERT_TRUE(Cbc::Format::CheckedCastOp::FromPair(entry.FromType(), entry.ToType()).has_value());
        EXPECT_EQ(Cbc::Format::CheckedCastOp::FromPair(entry.FromType(), entry.ToType())->Raw(), i);
    }
}

TEST(CbcRtDisasmTest, ConvertUnchecked)
{
    Cbc::RT::ConvertOp args {
        .opc  = Cbc::RT::Opcode::CONVERT,
        .cast = Cbc::Format::CastOp::I64_TO_I32,
        .rr   = { .x = Cbc::IReg::IR1, .y = Cbc::IReg::IR2 },
    };
    EXPECT_EQ("convert I64->I32 IR1 IR2\n", RtLog(args));
}

TEST(CbcRtDisasmTest, ConvertChecked)
{
    Cbc::RT::CheckedConvertOp args {
        .opc  = Cbc::RT::Opcode::CHECKED_CONVERT,
        .cast = Cbc::Format::CheckedCastOp::I64_TO_I32,
        .rr   = { .x = Cbc::IReg::IR1, .y = Cbc::IReg::IR2 },
    };
    EXPECT_EQ("checked.convert I64->I32 IR1 IR2\n", RtLog(args));
}
