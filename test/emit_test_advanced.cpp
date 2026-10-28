#include <cstdlib>
#include <string_view>

#include "emit_test_common.h"

namespace Cbc {
namespace Emitter {

using namespace Cbc::Format;

static constexpr int MAX_I12 = Cbc::RT::LIT_TABLE_SIZE / 2 - 1;
static constexpr int MIN_I12 = -Cbc::RT::LIT_TABLE_SIZE / 2;

// ---------------------------------------------------------------------------
// Literal table
// ---------------------------------------------------------------------------
TEST(EmitTest, Literals_None)
{
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

TEST(EmitTest, Literals_Label)
{
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

TEST(EmitTest, Literals_NoneBackEdge)
{
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

TEST(EmitTest, Literals_LabelBackEdge)
{
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

TEST(EmitTest, Simple_BccImm)
{
    Emitter e;
    auto loop = e.NewLabel();
    auto fwd  = e.NewLabel();
    auto end  = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.BccImm(CC::NE, Width::W32, IReg::IR1, 0xffff, fwd); // lit value, lit offset
    for (int i = 0; i < INT16_MAX / 8; ++i) {
        e.Ret();
    }
    e.Bind(fwd);
    e.BccImm(CC::LT, Width::W32, IReg::IR1, 100, loop);                           // lit neg offset
    e.BccImm(CC::LT, Width::W32, IReg::IR1, static_cast<uint64_t>(-0xffff), end); // lit neg value, false res
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.BccImm(CC::GE, Width::W32, IReg::IR1, static_cast<uint64_t>(-5), end); // neg value
    e.Ret();
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 2);
    e.Bind(end);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ(res.u32, 101);
}

// ---------------------------------------------------------------------------
// Integer binary operations (BIN32/BIN64 family)
// ---------------------------------------------------------------------------
TEST(EmitTest, BinaryOps_Reg)
{
    struct Case {
        Format::Common op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::Common::SUB,  Width::W64, "sub.64"  },
        { Format::Common::MUL,  Width::W32, "mul.32"  },
        { Format::Common::AND,  Width::W64, "and.64"  },
        { Format::Common::OR,   Width::W64, "or.64"   },
        { Format::Common::XOR,  Width::W64, "xor.64"  },
        { Format::Common::SDIV, Width::W64, "sdiv.64" },
        { Format::Common::SREM, Width::W64, "srem.64" },
        { Format::Common::UDIV, Width::W64, "udiv.64" },
        { Format::Common::UREM, Width::W64, "urem.64" },
        { Format::Common::LSL,  Width::W64, "lsl.64"  },
        { Format::Common::LSR,  Width::W64, "lsr.64"  },
        { Format::Common::ASR,  Width::W64, "asr.64"  },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.Binary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 12; ++i) {
        expected += Pos(3 * i) + ": " + cases[i].text + " IR1 IR2 IR3\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

TEST(EmitTest, BinaryOps_Imm)
{
    struct Case {
        Format::Common op;
        const char* text;
    } cases[] = {
        { Format::Common::SUB,  "sub.64"  },
        { Format::Common::MUL,  "mul.64"  },
        { Format::Common::AND,  "and.64"  },
        { Format::Common::OR,   "or.64"   },
        { Format::Common::XOR,  "xor.64"  },
        { Format::Common::SDIV, "sdiv.64" },
        { Format::Common::SREM, "srem.64" },
        { Format::Common::UDIV, "udiv.64" },
        { Format::Common::UREM, "urem.64" },
        { Format::Common::LSL,  "lsl.64"  },
        { Format::Common::LSR,  "lsr.64"  },
        { Format::Common::ASR,  "asr.64"  },
    };

    Emitter e;
    uint64_t imm = 1;
    for (const auto& test : cases) {
        e.BinaryImm(test.op, Width::W64, IReg::IR1, IReg::IR2, imm);
        imm++;
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 12; ++i) {
        expected += Pos(4 * i) + ": " + cases[i].text + " IR1 IR2 " + std::to_string(i + 1) + "\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

// ---------------------------------------------------------------------------
// Checked integer operations (CBIN family)
// ---------------------------------------------------------------------------
TEST(EmitTest, CheckedBinaryOps_Reg)
{
    struct Case {
        Format::Checked op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::Checked::CADD,  Width::W32, "cadd.32"  },
        { Format::Checked::CADD,  Width::W64, "cadd.64"  },
        { Format::Checked::CADD,  Width::W8,  "cadd.8"   },
        { Format::Checked::CADD,  Width::W16, "cadd.16"  },
        { Format::Checked::CSUB,  Width::W32, "csub.32"  },
        { Format::Checked::CSUB,  Width::W8,  "csub.8"   },
        { Format::Checked::CMUL,  Width::W64, "cmul.64"  },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64"  },
        { Format::Checked::CUADD, Width::W64, "cuadd.64" },
        { Format::Checked::CUSUB, Width::W64, "cusub.64" },
        { Format::Checked::CUMUL, Width::W64, "cumul.64" },
        { Format::Checked::CPOW,  Width::W64, "cpow.64"  },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.Binary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 12; ++i) {
        expected += Pos(3 * i) + ": " + cases[i].text + " IR1 IR2 IR3\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

TEST(EmitTest, CheckedBinaryOps_ImmShort)
{
    struct Case {
        Format::Checked op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::Checked::CADD,  Width::W8,  "cadd.8"   },
        { Format::Checked::CADD,  Width::W16, "cadd.16"  },
        { Format::Checked::CADD,  Width::W32, "cadd.32"  },
        { Format::Checked::CADD,  Width::W64, "cadd.64"  },
        { Format::Checked::CSUB,  Width::W32, "csub.32"  },
        { Format::Checked::CMUL,  Width::W32, "cmul.32"  },
        { Format::Checked::CUADD, Width::W32, "cuadd.32" },
        { Format::Checked::CUSUB, Width::W32, "cusub.32" },
        { Format::Checked::CUMUL, Width::W32, "cumul.32" },
        { Format::Checked::CPOW,  Width::W32, "cpow.32"  },
     };

    Emitter e;
    for (const auto& test : cases) {
        e.BinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, 3);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 10; ++i) {
        expected += Pos(4 * i) + ": " + cases[i].text + " IR1 IR2 3\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

TEST(EmitTest, CheckedBinaryOps_ImmWide)
{
    // Immediates that do not fit into 12 bits use the wide encoding.
    struct Case {
        Format::Checked op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::Checked::CADD,  Width::W8,  "cadd.8"   },
        { Format::Checked::CADD,  Width::W16, "cadd.16"  },
        { Format::Checked::CADD,  Width::W32, "cadd.32"  },
        { Format::Checked::CADD,  Width::W64, "cadd.64"  },
        { Format::Checked::CSUB,  Width::W64, "csub.64"  },
        { Format::Checked::CMUL,  Width::W32, "cmul.32"  },
        { Format::Checked::CUADD, Width::W32, "cuadd.32" },
        { Format::Checked::CUSUB, Width::W32, "cusub.32" },
        { Format::Checked::CUMUL, Width::W32, "cumul.32" },
        { Format::Checked::CPOW,  Width::W32, "cpow.32"  },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.BinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, 0x1000);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 10; ++i) {
        expected += Pos(11 * i) + ": " + cases[i].text + " IR1 IR2 4096\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

// ---------------------------------------------------------------------------
// Saturating integer operations (SBIN family)
// ---------------------------------------------------------------------------
TEST(EmitTest, SaturatingBinaryOps_Reg)
{
    struct Case {
        Format::Saturating op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::Saturating::SADD, Width::W32, "sadd.32" },
        { Format::Saturating::SADD, Width::W8,  "sadd.8"  },
        { Format::Saturating::SADD, Width::W16, "sadd.16" },
        { Format::Saturating::SADD, Width::W64, "sadd.64" },
        { Format::Saturating::SSUB, Width::W32, "ssub.32" },
        { Format::Saturating::SMUL, Width::W64, "smul.64" },
        { Format::Saturating::SDIV, Width::W64, "sdiv.64" },
        { Format::Saturating::SMOD, Width::W64, "smod.64" },
        { Format::Saturating::SPOW, Width::W64, "spow.64" },
        { Format::Saturating::SLSL, Width::W64, "slsl.64" },
        { Format::Saturating::SASR, Width::W64, "sasr.64" },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.SatBinary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 11; ++i) {
        expected += Pos(3 * i) + ": " + cases[i].text + " IR1 IR2 IR3\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

TEST(EmitTest, SaturatingBinaryOps_Imm)
{
    struct Case {
        Format::Saturating op;
        Width width;
        uint64_t imm;
        bool wide;
        const char* text;
        uint32_t size;
    } cases[] = {
        { Format::Saturating::SADD, Width::W32, 5,      false, "sadd.32", 4  },
        { Format::Saturating::SADD, Width::W8,  2,      false, "sadd.8",  4  },
        { Format::Saturating::SADD, Width::W16, 300,    false, "sadd.16", 4  },
        { Format::Saturating::SADD, Width::W64, 2000,   false, "sadd.64", 4  },
        { Format::Saturating::SMUL, Width::W32, 0x1000, true , "smul.32", 11 },
        { Format::Saturating::SMUL, Width::W8,  0x1000, true , "smul.8" , 11 },
        { Format::Saturating::SMUL, Width::W16, 0x1000, true , "smul.16", 11 },
        { Format::Saturating::SMUL, Width::W64, 0x1000, true , "smul.64", 11 },
        { Format::Saturating::SLSL, Width::W64, 3,      false, "slsl.64", 4  },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.SatBinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, test.imm);
    }
    auto code = e.Build(heap);

    size_t position = 0;
    std::string expected;
    for (const auto& test : cases) {
        // Short immediates print as signed 12-bit values, wide ones as I64.
        auto value = test.wide ? std::to_string(test.imm)
                               : std::to_string(static_cast<int>(MathUtils::SignExtend(test.imm, 12)));
        expected += Pos(position) + ": " + test.text + " IR1 IR2 " + value + "\n";
        position += test.size;
    }
    EXPECT_EQ(LogCode(code), expected);
}

// ---------------------------------------------------------------------------
// Float binary operations (FBIN family)
// ---------------------------------------------------------------------------
TEST(EmitTest, FloatBinaryOps)
{
    struct Case {
        Format::FloatOperations op;
        Width width;
        const char* text;
    } cases[] = {
        { Format::FloatOperations::FSUB,  Width::W32, "fsub.32"  },
        { Format::FloatOperations::FMUL,  Width::W64, "fmul.64"  },
        { Format::FloatOperations::FDIV,  Width::W64, "fdiv.64"  },
        { Format::FloatOperations::FMOV,  Width::W32, "fmov.32"  },
        { Format::FloatOperations::FNEG,  Width::W64, "fneg.64"  },
        { Format::FloatOperations::FABS,  Width::W32, "fabs.32"  },
        { Format::FloatOperations::FSQRT, Width::W64, "fsqrt.64" },
    };

    Emitter e;
    for (const auto& test : cases) {
        e.Binary(test.op, test.width, FReg::FR1, FReg::FR2, FReg::FR3);
    }
    auto code = e.Build(heap);

    std::string expected;
    for (size_t i = 0; i < 7; ++i) {
        expected += Pos(3 * i) + ": " + cases[i].text + " FR1 FR2 FR3\n";
    }
    EXPECT_EQ(LogCode(code), expected);
}

// ---------------------------------------------------------------------------
// LOG pseudo instruction
// ---------------------------------------------------------------------------
TEST(EmitTest, LogInstruction)
{
    Emitter e;
    e.LogInstruction(std::string_view("hello"));
    e.Ret();

    auto code = e.Build(heap);

    // Executing the LOG instruction must print the recorded string.
    testing::internal::CaptureStderr();
    Interpret(code, U32(0), U32(0));
    auto out = testing::internal::GetCapturedStderr();
    EXPECT_NE(out.find("hello"), std::string::npos);

    // LogInstruction(std::string_view) takes ownership of a malloc'd copy that
    // lives as long as the code object; the test is its only owner in this
    // process, so release it after execution to keep LeakSanitizer clean.
    // Decoding is the only way back to the pointer — used here for cleanup,
    // not for assertions.
    Decoder::ByteReader reader = MakeReader(code);
    auto logInstr              = RT::B9i64::Decode(reader);
    free(reinterpret_cast<char*>(logInstr.imm64.ptr));
}

TEST(EmitTest, LogInstruction_CharOverload)
{
    Emitter e;
    char buffer[] = "dynamic";
    e.LogInstruction(buffer);
    e.Ret();

    auto code = e.Build(heap);

    // The char* overload stores the caller's pointer as-is (no copy), so
    // executing the LOG instruction prints the test-owned buffer.
    testing::internal::CaptureStderr();
    Interpret(code, U32(0), U32(0));
    auto out = testing::internal::GetCapturedStderr();
    EXPECT_NE(out.find("dynamic"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Branch variants not covered elsewhere
// ---------------------------------------------------------------------------
TEST(EmitTest, Bcc64_ShortOffsets)
{
    {
        Emitter e; // forward short branch -> BCC64I
        auto label = e.NewLabel();
        e.Bcc(CC::EQ, Width::W64, IReg::IR1, IReg::IR2, label);
        e.Ret();
        e.Bind(label);

        auto code = e.Build(heap);
        CHECK_DISASM(
            code,
            "0x000: bcc.64 eq IR1 IR2 1",
            "0x004: ret"
        );
    }
    {
        Emitter e; // backward short branch -> still BCC64I (I = immediate offset)
        auto label = e.NewLabel();
        e.Bind(label);
        e.Ret();
        e.Bcc(CC::NE, Width::W64, IReg::IR1, IReg::IR2, label);

        auto code = e.Build(heap);
        CHECK_DISASM(
            code,
            "0x000: ret",
            "0x001: bcc.64 ne IR1 IR2 -5"
        );
    }
}

TEST(EmitTest, Bcc64_FarBranch)
{
    Emitter e; // branch offset beyond 12 bits -> BCC64L with a literal offset
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W64, IReg::IR1, IReg::IR2, label);
    for (int i = 0; i < 2100; ++i) {
        e.Ret();
    }
    e.Bind(label);

    LimitedHeap<8192> h;
    auto code = e.Build(h);
    ASSERT_EQ(code.literals->size(), 1u);
    EXPECT_EQ(code.literals->at(0).i64, 2100);

    std::string prefix = "0x000: bcc.64 eq IR1 IR2 2100\n";
    EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
}

TEST(EmitTest, BccImm64_Variants)
{
    {
        Emitter e; // imm offset + imm value -> BCCI64I
        auto label = e.NewLabel();
        e.BccImm(CC::NE, Width::W64, IReg::IR1, 5, label);
        e.Ret();
        e.Ret();
        e.Bind(label);

        auto code = e.Build(heap);
        CHECK_DISASM(
            code,
            "0x000: bcc.64 ne IR1 5 2",
            "0x005: ret",
            "0x006: ret"
        );
    }
    {
        Emitter e; // imm offset + literal value -> BCCL64I
        auto label = e.NewLabel();
        e.BccImm(CC::NE, Width::W64, IReg::IR1, 65536, label);
        e.Ret();
        e.Bind(label);

        auto code = e.Build(heap);
        ASSERT_EQ(code.literals->size(), 1u);
        CHECK_DISASM(
            code,
            "0x000: bcc.64 ne IR1 65536 1",
            "0x005: ret"
        );
    }
    {
        Emitter e; // literal offset + imm value -> BCCI64L
        auto label = e.NewLabel();
        e.BccImm(CC::LT, Width::W64, IReg::IR1, 5, label);
        for (int i = 0; i < 2100; ++i) {
            e.Ret();
        }
        e.Bind(label);

        LimitedHeap<8192> h;
        auto code = e.Build(h);
        ASSERT_EQ(code.literals->size(), 1u);
        EXPECT_EQ(code.literals->at(0).i64, 2100);

        std::string prefix = "0x000: bcc.64 lt IR1 5 2100\n";
        EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
    }
}

TEST(EmitTest, BccImm_BothLiteral)
{
    {
        Emitter e; // literal offset + literal value -> BCCL32L
        auto label = e.NewLabel();
        e.BccImm(CC::NE, Width::W32, IReg::IR1, 131072, label);
        for (int i = 0; i < 2100; ++i) {
            e.Ret();
        }
        e.Bind(label);

        LimitedHeap<8192> h;
        auto code = e.Build(h);
        ASSERT_EQ(code.literals->size(), 2u);
        // Value literal (131072) resolved first in the text, then the offset
        // literal (2100); the rest of the code is padding `ret`s.
        std::string prefix = "0x000: bcc.32 ne IR1 131072 2100\n";
        EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
    }
    {
        Emitter e; // literal offset + literal value -> BCCL64L
        auto label = e.NewLabel();
        e.BccImm(CC::LT, Width::W64, IReg::IR1, 131072, label);
        for (int i = 0; i < 2100; ++i) {
            e.Ret();
        }
        e.Bind(label);

        LimitedHeap<8192> h;
        auto code = e.Build(h);
        ASSERT_EQ(code.literals->size(), 2u);
        std::string prefix = "0x000: bcc.64 lt IR1 131072 2100\n";
        EXPECT_EQ(LogCode(code).substr(0, prefix.size()), prefix);
    }
}

// ---------------------------------------------------------------------------
// Stack slot copies
// ---------------------------------------------------------------------------
TEST(EmitTest, SSC)
{
    struct {
        CC cc;
        Width width;
        uint32_t expected;
    } cases[] = {
        { CC::EQ, Width::W32, 0 },    { CC::NE, Width::W32, 1 },     { CC::LT, Width::W32, 0 },
        { CC::GE, Width::W32, 1 },    { CC::ULT, Width::W32, 0 },    { CC::UGE, Width::W32, 1 },
        { CC::TESTZ, Width::W32, 1 }, { CC::TESTNZ, Width::W32, 0 },

        { CC::EQ, Width::W64, 0 },    { CC::NE, Width::W64, 1 },     { CC::LT, Width::W64, 0 },
        { CC::GE, Width::W64, 1 },    { CC::ULT, Width::W64, 0 },    { CC::UGE, Width::W64, 1 },
        { CC::TESTZ, Width::W64, 1 }, { CC::TESTNZ, Width::W64, 0 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SCC(test.cc, test.width, IReg::IR1, IReg::IR1, IReg::IR2);
        e.Ret();

        auto code = e.Build(heap);
        Cbc::RT::Log(code, Stream::Disasm::rt);

        auto res = Interpret(code, U64(4), U64(3));
        EXPECT_EQ(res.u32, test.expected);
    }
}

TEST(EmitTest, FSSC32)
{
    struct {
        CC cc;
        float l;
        float r;
        uint32_t expected;
    } cases[] = {
        { CC::FEQ, 4.2, 3.1, 0 },           { CC::FNE, 4.2, 3.1, 1 },
        { CC::FLT, 4.2, 3.1, 0 },           { CC::FNLT, 4.2, 3.1, 1 },
        { CC::FGE, 4.2, 3.1, 1 },           { CC::FNGE, 4.2, 3.1, 0 },

        { CC::FEQ, NAN, NAN, 0 },           { CC::FNE, NAN, NAN, 1 },
        { CC::FLT, NAN, NAN, 0 },           { CC::FNLT, NAN, NAN, 1 },
        { CC::FGE, NAN, NAN, 0 },           { CC::FNGE, NAN, NAN, 1 },

        { CC::FEQ, INFINITY, INFINITY, 1 }, { CC::FNE, INFINITY, INFINITY, 0 },
        { CC::FLT, INFINITY, INFINITY, 0 }, { CC::FNLT, INFINITY, INFINITY, 1 },
        { CC::FGE, INFINITY, INFINITY, 1 }, { CC::FNGE, INFINITY, INFINITY, 0 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SCC(test.cc, Width::W32, IReg::IR1, FReg::FR0, FReg::FR1);
        e.Mov(FReg::FR0, IReg::IR1);
        e.Ret();

        auto code = e.Build(heap);
        Cbc::RT::Log(code, Stream::Disasm::rt);

        auto res = InterpretFPRes(code, F32(test.l), F32(test.r));
        EXPECT_EQ(res.u32, test.expected);
    }
}

TEST(EmitTest, FSSC64)
{
    struct {
        CC cc;
        double l;
        double r;
        uint32_t expected;
    } cases[] = {
        { CC::FEQ, 4.2, 3.1, 0 },           { CC::FNE, 4.2, 3.1, 1 },
        { CC::FLT, 4.2, 3.1, 0 },           { CC::FNLT, 4.2, 3.1, 1 },
        { CC::FGE, 4.2, 3.1, 1 },           { CC::FNGE, 4.2, 3.1, 0 },

        { CC::FEQ, NAN, NAN, 0 },           { CC::FNE, NAN, NAN, 1 },
        { CC::FLT, NAN, NAN, 0 },           { CC::FNLT, NAN, NAN, 1 },
        { CC::FGE, NAN, NAN, 0 },           { CC::FNGE, NAN, NAN, 1 },

        { CC::FEQ, INFINITY, INFINITY, 1 }, { CC::FNE, INFINITY, INFINITY, 0 },
        { CC::FLT, INFINITY, INFINITY, 0 }, { CC::FNLT, INFINITY, INFINITY, 1 },
        { CC::FGE, INFINITY, INFINITY, 1 }, { CC::FNGE, INFINITY, INFINITY, 0 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SCC(test.cc, Width::W64, IReg::IR1, FReg::FR0, FReg::FR1);
        e.Mov(FReg::FR0, IReg::IR1);
        e.Ret();

        auto code = e.Build(heap);
        Cbc::RT::Log(code, Stream::Disasm::rt);

        auto res = InterpretFPRes(code, F64(test.l), F64(test.r));
        EXPECT_EQ(res.u32, test.expected);
    }
}

TEST(EmitTest, SSCI32)
{
    struct {
        CC cc;
        Width width;
        uint64_t imm;
        uint32_t expected;
    } cases[] = {
        { CC::EQ, Width::W32, 0xff, 0 },      { CC::NE, Width::W32, 0xff, 1 },       { CC::LT, Width::W32, 0xff, 0 },
        { CC::GE, Width::W32, 0xff, 1 },      { CC::ULT, Width::W32, 0xff, 0 },      { CC::UGE, Width::W32, 0xff, 1 },
        { CC::TESTZ, Width::W32, 0xff, 0 },   { CC::TESTNZ, Width::W32, 0xff, 1 },

        { CC::EQ, Width::W64, 0xff, 0 },      { CC::NE, Width::W64, 0xff, 1 },       { CC::LT, Width::W64, 0xff, 0 },
        { CC::GE, Width::W64, 0xff, 1 },      { CC::ULT, Width::W64, 0xff, 0 },      { CC::UGE, Width::W64, 0xff, 1 },
        { CC::TESTZ, Width::W64, 0xff, 0 },   { CC::TESTNZ, Width::W64, 0xff, 1 },

        { CC::EQ, Width::W32, 0xff00, 0 },    { CC::NE, Width::W32, 0xff00, 1 },     { CC::LT, Width::W32, 0xff00, 0 },
        { CC::GE, Width::W32, 0xff00, 1 },    { CC::ULT, Width::W32, 0xff00, 0 },    { CC::UGE, Width::W32, 0xff00, 1 },
        { CC::TESTZ, Width::W32, 0xff00, 0 }, { CC::TESTNZ, Width::W32, 0xff00, 1 },

        { CC::EQ, Width::W64, 0xff00, 0 },    { CC::NE, Width::W64, 0xff00, 1 },     { CC::LT, Width::W64, 0xff00, 0 },
        { CC::GE, Width::W64, 0xff00, 1 },    { CC::ULT, Width::W64, 0xff00, 0 },    { CC::UGE, Width::W64, 0xff00, 1 },
        { CC::TESTZ, Width::W64, 0xff00, 0 }, { CC::TESTNZ, Width::W64, 0xff00, 1 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SCCImm(test.cc, test.width, IReg::IR1, IReg::IR2, test.imm);
        e.Ret();

        auto code = e.Build(heap);
        Cbc::RT::Log(code, Stream::Disasm::rt);

        auto res = Interpret(code, U64(0), U64(0xffff));
        EXPECT_EQ(res.u32, test.expected);
    }
}

// ---------------------------------------------------------------------------
// Bit field extraction
// ---------------------------------------------------------------------------
static void testBFX(bool signExtend)
{
    struct {
        uint64_t value;
        uint64_t from;
        uint32_t to;
    } cases[] = {
        { 0x3EFL, 3, 5 },
        { 0x2F0L, 7, 9 },

        { 0x3FCL, 0, 2 },
        { 0xFFFFFFFFL, 29, 31 },

        { 0x3BCL, 6, 6 },
        { 0x3BCL, 5, 5 },

        { 0xABCDE3BA29AFCCB7L, 0, 31 },
        { 0xABCDE3BA29AFCCB7L, 43, 52 },

        { 0xFFFFFFFFL, 0, 31 },
        { 0xFFFFFFFFFFFFFFFFL, 0, 31 },
        { 0xFFFFFFFFFFFFFFFFL, 0, 63 },
    };

    for (const auto& test : cases) {
        auto size = test.to - test.from + 1;
        auto expectedBits = MathUtils::Bits(test.value, test.from, test.to);
        auto expected = signExtend
            ? MathUtils::SignExtend(expectedBits, size)
            : MathUtils::ZeroExtend(expectedBits, size);

        Emitter e;
        if (signExtend) {
            e.BFXS(IReg::IR1, IReg::IR2, test.from, size);
        } else {
            e.BFXZ(IReg::IR1, IReg::IR2, test.from, size);
        }
        e.Ret();

        auto code = e.Build(heap);
        Cbc::RT::Log(code, Stream::Disasm::rt);

        auto res = Interpret(code, U64(0xBAADF00DL), U64(test.value));
        EXPECT_EQ(res.u64, expected);
    }
}

TEST(EmitTest, BFXS)
{
    testBFX(true);
}

TEST(EmitTest, BFXZ)
{
    testBFX(false);
}

} // namespace Emitter
} // namespace Cbc
