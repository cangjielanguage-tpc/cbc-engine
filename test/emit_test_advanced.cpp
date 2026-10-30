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
        int64_t l;
        int64_t r;
        int64_t expected;
    } cases[] = {
        { Format::Common::SUB,  Width::W64, "sub.64",  10,    4,    6 },
        { Format::Common::MUL,  Width::W32, "mul.32",  6,     7,    42 },
        { Format::Common::AND,  Width::W64, "and.64",  0xCC,  0x0F, 0x0C },
        { Format::Common::OR,   Width::W64, "or.64",   0xC0,  0x0F, 0xCF },
        { Format::Common::XOR,  Width::W64, "xor.64",  0xFF,  0x0F, 0xF0 },
        { Format::Common::SDIV, Width::W64, "sdiv.64", -7,    2,    -3 },
        { Format::Common::SREM, Width::W64, "srem.64", -7,    2,    -1 },
        { Format::Common::UDIV, Width::W64, "udiv.64", 42,    5,    8 },
        { Format::Common::UREM, Width::W64, "urem.64", 42,    5,    2 },
        { Format::Common::LSL,  Width::W64, "lsl.64",  1,     5,    32 },
        { Format::Common::LSR,  Width::W64, "lsr.64",  static_cast<int64_t>(0x8000000000000000ull), 4,
          static_cast<int64_t>(0x0800000000000000ull) },
        { Format::Common::ASR,  Width::W64, "asr.64",  -16,   2,    -4 },
        { Format::Common::SDIV, Width::W64, "sdiv.64", INT64_MIN, -1, INT64_MIN },
        { Format::Common::SREM, Width::W64, "srem.64", INT64_MIN, -1, 0 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.Binary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: " + std::string(test.text) + " IR1 IR2 IR3", "0x003: ret");
        ExpectResult(Interpret(code, U64(0), U64(test.l), U64(test.r)), test.width, test.expected);
    }

    // div/rem with zero divisor intentionally FATALs
    // (compiler always cover with divisor check)
    // so no zero division tests
    struct ThrowCase {
        Format::Common op;
        const char* text;
    } throwCases[] = {
        { Format::Common::SDIV, "sdiv.64" },
        { Format::Common::SREM, "srem.64" },
        { Format::Common::UDIV, "udiv.64" },
        { Format::Common::UREM, "urem.64" },
    };

    for (const auto& test : throwCases) {
        Emitter e;
        e.DivCheck(IReg::IR3);
        e.Binary(test.op, Width::W64, IReg::IR1, IReg::IR2, IReg::IR3);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: divcheck IR3", "0x002: " + std::string(test.text) + " IR1 IR2 IR3", "0x005: ret");
        EXPECT_EQ(InterpretThrows(code, U64(10), U64(7), U64(0)), Interpretation::Type::ArithmeticException);
    }
}

TEST(EmitTest, BinaryOps_Imm)
{
    struct Case {
        Format::Common op;
        const char* text;
        int64_t imm;
        int64_t l;
        int64_t expected;
    } cases[] = {
        { Format::Common::SUB,  "sub.64",  10,   100,       90 },
        { Format::Common::MUL,  "mul.64",  3,    14,        42 },
        { Format::Common::AND,  "and.64",  0x0F, 0xCC,      0x0C },
        { Format::Common::OR,   "or.64",   0x0F, 0xC0,      0xCF },
        { Format::Common::XOR,  "xor.64",  0xFF, 0x0F,      0xF0 },
        { Format::Common::SDIV, "sdiv.64", 2,    -7,        -3 },
        { Format::Common::SREM, "srem.64", 2,    -7,        -1 },
        { Format::Common::UDIV, "udiv.64", 5,    42,        8 },
        { Format::Common::UREM, "urem.64", 5,    42,        2 },
        { Format::Common::LSL,  "lsl.64",  5,    1,         32 },
        { Format::Common::LSR,  "lsr.64",  4,    INT64_MIN, static_cast<int64_t>(0x0800000000000000ull) },
        { Format::Common::ASR,  "asr.64",  2,    -16,       -4 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.BinaryImm(test.op, Width::W64, IReg::IR1, IReg::IR2, static_cast<uint64_t>(test.imm));
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(
            code, "0x000: " + std::string(test.text) + " IR1 IR2 " + std::to_string(test.imm), "0x004: ret"
        );
        ExpectResult(Interpret(code, U64(0), U64(test.l)), Width::W64, test.expected);
    }
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
        int64_t l;
        int64_t r;
        int64_t expected;
        Interpretation::Type exception;
    } cases[] = {
        { Format::Checked::CADD,  Width::W8,  "cadd.8",   100,        27,  127, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W8,  "cadd.8",   120,        10,  0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W16, "cadd.16",  30000,      1000, 31000, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W16, "cadd.16",  32767,      1,   0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  5,          7,   12,  Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  INT32_MAX,  1,   0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  5,          7,   12,  Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  INT64_MAX,  1,   0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CSUB,  Width::W8,  "csub.8",   5,          7,   -2,  Interpretation::Type::Successful },
        { Format::Checked::CSUB,  Width::W8,  "csub.8",   INT8_MIN,   1,   0,   Interpretation::Type::SubOverflowException },
        { Format::Checked::CSUB,  Width::W32, "csub.32",  5,          7,   -2,  Interpretation::Type::Successful },
        { Format::Checked::CSUB,  Width::W32, "csub.32",  INT32_MIN,  1,   0,   Interpretation::Type::SubOverflowException },
        { Format::Checked::CMUL,  Width::W64, "cmul.64",  6,          7,   42,  Interpretation::Type::Successful },
        { Format::Checked::CMUL,  Width::W64, "cmul.64",  4294967296, 4294967296, 0, Interpretation::Type::MulOverflowException },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  42,         7,   6,   Interpretation::Type::Successful },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  7,          0,   0,   Interpretation::Type::ArithmeticException },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  INT64_MIN,  -1,  0,   Interpretation::Type::DivOverflowException },
        { Format::Checked::CUADD, Width::W64, "cuadd.64", 5,          7,   12,  Interpretation::Type::Successful },
        { Format::Checked::CUADD, Width::W64, "cuadd.64", -1,         1,   0,   Interpretation::Type::UaddOverflowException },
        { Format::Checked::CUSUB, Width::W64, "cusub.64", 7,          5,   2,   Interpretation::Type::Successful },
        { Format::Checked::CUSUB, Width::W64, "cusub.64", 0,          1,   0,   Interpretation::Type::UsubOverflowException },
        { Format::Checked::CUMUL, Width::W64, "cumul.64", 6,          7,   42,  Interpretation::Type::Successful },
        { Format::Checked::CUMUL, Width::W64, "cumul.64", 4294967296, 4294967296, 0, Interpretation::Type::UmulOverflowException },
        { Format::Checked::CPOW,  Width::W64, "cpow.64",  2,          10,  1024, Interpretation::Type::Successful },
        { Format::Checked::CPOW,  Width::W64, "cpow.64",  2,          63,  0,   Interpretation::Type::UpowOverflowException },
        { Format::Checked::CLSL,  Width::W64, "clsl.64",  1,          5,   32,  Interpretation::Type::Successful },
        { Format::Checked::CLSL,  Width::W64, "clsl.64",  1,          64,  0,   Interpretation::Type::LshOverflowException },
        { Format::Checked::CLSR,  Width::W64, "clsr.64",  240,        4,   15,  Interpretation::Type::Successful },
        { Format::Checked::CLSR,  Width::W64, "clsr.64",  1,          64,  0,   Interpretation::Type::RshOverflowException },
        { Format::Checked::CASR,  Width::W64, "casr.64",  -16,        2,   -4,  Interpretation::Type::Successful },
        { Format::Checked::CASR,  Width::W64, "casr.64",  -1,         64,  0,   Interpretation::Type::AshOverflowException },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.Binary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: " + std::string(test.text) + " IR1 IR2 IR3", "0x003: ret");
        if (test.exception == Interpretation::Type::Successful) {
            ExpectResult(Interpret(code, U64(0), U64(test.l), U64(test.r)), test.width, test.expected);
        } else {
            EXPECT_EQ(InterpretThrows(code, U64(0), U64(test.l), U64(test.r)), test.exception);
        }
    }
}

TEST(EmitTest, CheckedBinaryOps_ImmShort)
{
    struct Case {
        Format::Checked op;
        Width width;
        const char* text;
        int64_t imm;
        int64_t l;
        int64_t expected;
        Interpretation::Type exception;
    } cases[] = {
        { Format::Checked::CADD,  Width::W8,  "cadd.8",   3, 100,       103, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W8,  "cadd.8",   3, 126,       0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W16, "cadd.16",  3, 30000,     30003, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W16, "cadd.16",  3, 32767,     0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  3, 100,       103, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  3, INT32_MAX, 0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  3, 100,       103, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  3, INT64_MAX, 0,   Interpretation::Type::AddOverflowException },
        { Format::Checked::CSUB,  Width::W32, "csub.32",  3, 100,       97,  Interpretation::Type::Successful },
        { Format::Checked::CSUB,  Width::W32, "csub.32",  3, INT32_MIN, 0,   Interpretation::Type::SubOverflowException },
        { Format::Checked::CMUL,  Width::W32, "cmul.32",  3, 7,         21,  Interpretation::Type::Successful },
        { Format::Checked::CMUL,  Width::W32, "cmul.32",  3, 0x40000000, 0,  Interpretation::Type::MulOverflowException },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  7, 42,        6,   Interpretation::Type::Successful },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  0, 7,         0,   Interpretation::Type::ArithmeticException },
        { Format::Checked::CDIV,  Width::W64, "cdiv.64",  -1, INT64_MIN, 0,  Interpretation::Type::DivOverflowException },
        { Format::Checked::CUADD, Width::W32, "cuadd.32", 3, 7,         10,  Interpretation::Type::Successful },
        { Format::Checked::CUADD, Width::W32, "cuadd.32", 3, -1,        0,   Interpretation::Type::UaddOverflowException },
        { Format::Checked::CUSUB, Width::W32, "cusub.32", 3, 7,         4,   Interpretation::Type::Successful },
        { Format::Checked::CUSUB, Width::W32, "cusub.32", 3, 2,         0,   Interpretation::Type::UsubOverflowException },
        { Format::Checked::CUMUL, Width::W32, "cumul.32", 3, 7,         21,  Interpretation::Type::Successful },
        { Format::Checked::CUMUL, Width::W32, "cumul.32", 3, 2147483648, 0,  Interpretation::Type::UmulOverflowException },
        { Format::Checked::CPOW,  Width::W32, "cpow.32",  3, 7,         343, Interpretation::Type::Successful },
        { Format::Checked::CPOW,  Width::W32, "cpow.32",  3, 0x10000,   0,   Interpretation::Type::UpowOverflowException },
        { Format::Checked::CLSL,  Width::W32, "clsl.32",  3, 1,         8,   Interpretation::Type::Successful },
        { Format::Checked::CLSL,  Width::W32, "clsl.32",  32, 1,        0,   Interpretation::Type::LshOverflowException },
        { Format::Checked::CLSR,  Width::W32, "clsr.32",  3, 0xF0,      0x1E, Interpretation::Type::Successful },
        { Format::Checked::CLSR,  Width::W32, "clsr.32",  32, 1,        0,   Interpretation::Type::RshOverflowException },
        { Format::Checked::CASR,  Width::W32, "casr.32",  3, -16,       -2,  Interpretation::Type::Successful },
        { Format::Checked::CASR,  Width::W32, "casr.32",  32, -1,       0,   Interpretation::Type::AshOverflowException },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.BinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, static_cast<uint64_t>(test.imm));
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(
            code, "0x000: " + std::string(test.text) + " IR1 IR2 " + std::to_string(test.imm), "0x004: ret"
        );
        if (test.exception == Interpretation::Type::Successful) {
            ExpectResult(Interpret(code, U64(0), U64(test.l)), test.width, test.expected);
        } else {
            EXPECT_EQ(InterpretThrows(code, U64(0), U64(test.l)), test.exception);
        }
    }
}

TEST(EmitTest, CheckedBinaryOps_ImmWide)
{
    // Immediates that do not fit into 12 bits use the wide encoding.
    struct Case {
        Format::Checked op;
        Width width;
        const char* text;
        int64_t l;
        int64_t expected;
        Interpretation::Type exception;
    } cases[] = {
        // W8 wides do not exist (never emitteed by compiler, asserted in emitter)
        // so no W8 tests here
        { Format::Checked::CADD,  Width::W16, "cadd.16",  -4096,     0,    Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W16, "cadd.16",  32767,     0,    Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  -4096,     0,    Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W32, "cadd.32",  INT32_MAX, 0,    Interpretation::Type::AddOverflowException },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  1,         4097, Interpretation::Type::Successful },
        { Format::Checked::CADD,  Width::W64, "cadd.64",  INT64_MAX, 0,    Interpretation::Type::AddOverflowException },
        { Format::Checked::CSUB,  Width::W64, "csub.64",  8192,      4096, Interpretation::Type::Successful },
        { Format::Checked::CSUB,  Width::W64, "csub.64",  INT64_MIN, 0,    Interpretation::Type::SubOverflowException },
        { Format::Checked::CMUL,  Width::W32, "cmul.32",  1,         4096, Interpretation::Type::Successful },
        { Format::Checked::CMUL,  Width::W32, "cmul.32",  0x100000,  0,    Interpretation::Type::MulOverflowException },
        { Format::Checked::CUADD, Width::W32, "cuadd.32", 1,         4097, Interpretation::Type::Successful },
        { Format::Checked::CUADD, Width::W32, "cuadd.32", -1,        0,    Interpretation::Type::UaddOverflowException },
        { Format::Checked::CUSUB, Width::W32, "cusub.32", 8192,      4096, Interpretation::Type::Successful },
        { Format::Checked::CUSUB, Width::W32, "cusub.32", 4095,      0,    Interpretation::Type::UsubOverflowException },
        { Format::Checked::CUMUL, Width::W32, "cumul.32", 1,         4096, Interpretation::Type::Successful },
        { Format::Checked::CUMUL, Width::W32, "cumul.32", 0x100000,  0,    Interpretation::Type::UmulOverflowException },
        { Format::Checked::CPOW,  Width::W32, "cpow.32",  1,         1,    Interpretation::Type::Successful },
        { Format::Checked::CPOW,  Width::W32, "cpow.32",  2,         0,    Interpretation::Type::UpowOverflowException },
        { Format::Checked::CLSL,  Width::W32, "clsl.32",  1,         0,    Interpretation::Type::LshOverflowException },
        { Format::Checked::CLSR,  Width::W32, "clsr.32",  1,         0,    Interpretation::Type::RshOverflowException },
        { Format::Checked::CASR,  Width::W32, "casr.32",  -1,        0,    Interpretation::Type::AshOverflowException },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.BinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, 0x1000);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(
            code, "0x000: " + std::string(test.text) + " IR1 IR2 4096", "0x00b: ret"
        );
        if (test.exception == Interpretation::Type::Successful) {
            ExpectResult(Interpret(code, U64(0), U64(test.l)), test.width, test.expected);
        } else {
            EXPECT_EQ(InterpretThrows(code, U64(0), U64(test.l)), test.exception);
        }
    }
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
        int64_t l;
        int64_t r;
        int64_t expected;
    } cases[] = {
        { Format::Saturating::SADD, Width::W32, "sadd.32", 5,          7,  12 },
        { Format::Saturating::SADD, Width::W32, "sadd.32", INT32_MAX,  5,  INT32_MAX },
        { Format::Saturating::SADD, Width::W8,  "sadd.8",  120,        10, 127 },
        { Format::Saturating::SADD, Width::W16, "sadd.16", 32767,      1,  32767 },
        { Format::Saturating::SADD, Width::W64, "sadd.64", INT64_MAX,  1,  INT64_MAX },
        { Format::Saturating::SSUB, Width::W32, "ssub.32", INT32_MIN,  1,  INT32_MIN },
        { Format::Saturating::SMUL, Width::W64, "smul.64", 4294967296, 4294967296, INT64_MAX },
        { Format::Saturating::SDIV, Width::W64, "sdiv.64", 42,         7,  6 },
        { Format::Saturating::SDIV, Width::W64, "sdiv.64", INT64_MIN,  -1, INT64_MAX },
        { Format::Saturating::SMOD, Width::W64, "smod.64", 7,          3,  1 },
        { Format::Saturating::SMOD, Width::W64, "smod.64", INT64_MIN,  -1, 0 },
        { Format::Saturating::SPOW, Width::W64, "spow.64", 2,          10, 1024 },
        { Format::Saturating::SPOW, Width::W64, "spow.64", 3,          100, INT64_MAX },
        { Format::Saturating::SLSL, Width::W64, "slsl.64", 1,          5,  32 },
        { Format::Saturating::SLSL, Width::W64, "slsl.64", 1,          100, INT64_MAX },
        { Format::Saturating::SASR, Width::W64, "sasr.64", -16,        2,  -4 },
        { Format::Saturating::SASR, Width::W64, "sasr.64", -1,         100, -1 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SatBinary(test.op, test.width, IReg::IR1, IReg::IR2, IReg::IR3);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: " + std::string(test.text) + " IR1 IR2 IR3", "0x003: ret");
        ExpectResult(Interpret(code, U64(0), U64(test.l), U64(test.r)), test.width, test.expected);
    }
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
        int64_t l;
        int64_t expected;
    } cases[] = {
        { Format::Saturating::SADD, Width::W32, 5,      false, "sadd.32", 4,  100,       105 },
        { Format::Saturating::SADD, Width::W32, 5,      false, "sadd.32", 4,  INT32_MAX, INT32_MAX },
        { Format::Saturating::SADD, Width::W8,  2,      false, "sadd.8",  4,  126,       127 },
        { Format::Saturating::SADD, Width::W16, 300,    false, "sadd.16", 4,  32000,     32300 },
        { Format::Saturating::SADD, Width::W16, 300,    false, "sadd.16", 4,  32767,     32767 },
        { Format::Saturating::SADD, Width::W64, 2000,   false, "sadd.64", 4,  1,         2001 },
        { Format::Saturating::SMUL, Width::W32, 0x1000, true,  "smul.32", 11, 1,         4096 },
        { Format::Saturating::SMUL, Width::W32, 0x1000, true,  "smul.32", 11, 0x100000,  INT32_MAX },
        // Narrow-width immediates are normalized to the operation width by the
        // compiler before emission (an out-of-range one trips the emitter
        // assert), so W8 has no wide-immediate rows here.
        { Format::Saturating::SMUL, Width::W16, 0x1000, true,  "smul.16", 11, 1,         4096 },
        { Format::Saturating::SMUL, Width::W64, 0x1000, true,  "smul.64", 11, 1,         4096 },
        { Format::Saturating::SLSL, Width::W64, 3,      false, "slsl.64", 4,  1,         8 },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.SatBinaryImm(test.op, test.width, IReg::IR1, IReg::IR2, test.imm);
        e.Ret();
        auto code = e.Build(heap);
        // Short immediates print as signed 12-bit values, wide ones as I64.
        auto value = test.wide ? std::to_string(test.imm)
                               : std::to_string(static_cast<int>(MathUtils::SignExtend(test.imm, 12)));
        CHECK_DISASM(
            code, "0x000: " + std::string(test.text) + " IR1 IR2 " + value, Pos(test.size) + ": ret"
        );
        ExpectResult(Interpret(code, U64(0), U64(test.l)), test.width, test.expected);
    }
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
        double l;
        double r;
        double expected;
        bool interpret;
    } cases[] = {
        { Format::FloatOperations::FSUB,  Width::W32, "fsub.32",  4.5,   0.5,      4.0,       true },
        { Format::FloatOperations::FMUL,  Width::W64, "fmul.64",  3.0,   2.5,      7.5,       true },
        { Format::FloatOperations::FDIV,  Width::W64, "fdiv.64",  7.5,   2.5,      3.0,       true },
        { Format::FloatOperations::FDIV,  Width::W64, "fdiv.64",  1.0,   0.0,      INFINITY,  true },
        { Format::FloatOperations::FDIV,  Width::W64, "fdiv.64",  -1.0,  0.0,      -INFINITY, true },
        { Format::FloatOperations::FDIV,  Width::W32, "fdiv.32",  1.0,   0.0,      INFINITY,  true },
        { Format::FloatOperations::FMOV,  Width::W32, "fmov.32",  0,     0,        0,         false },
        { Format::FloatOperations::FNEG,  Width::W64, "fneg.64",  0,     0,        0,         false },
        { Format::FloatOperations::FABS,  Width::W32, "fabs.32",  0,     0,        0,         false },
        { Format::FloatOperations::FSQRT, Width::W64, "fsqrt.64", 0,     0,        0,         false },
    };

    for (const auto& test : cases) {
        Emitter e;
        e.Binary(test.op, test.width, FReg::FR0, FReg::FR0, FReg::FR1);
        e.Ret();
        auto code = e.Build(heap);
        CHECK_DISASM(code, "0x000: " + std::string(test.text) + " FR0 FR0 FR1", "0x003: ret");
        if (!test.interpret) {
            continue;
        }
        auto res = test.width == Width::W32
            ? InterpretFPRes(code, F32(static_cast<float>(test.l)), F32(static_cast<float>(test.r)))
            : InterpretFPRes(code, F64(test.l), F64(test.r));
        if (test.width == Width::W32) {
            EXPECT_EQ(res.f32, static_cast<float>(test.expected));
        } else {
            EXPECT_EQ(res.f64, test.expected);
        }
    }
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
