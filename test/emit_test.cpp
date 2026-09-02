#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/formater_rt.h"
#include "cbc/isa_rt.h"

#include "mock/interpreter.h"
#include "testutils.h"
#include "utils/ostream.h"

static LimitedHeap<16384> heap;

class EmitTest : public testing::Test {
    void SetUp() override
    {
        InitializeMockInterpreter();
        heap.Reset();
    }

    void TearDown() override {}
};

namespace Cbc {
namespace Emitter {

struct Test;

static constexpr int MAX_I12 = Cbc::RT::LIT_TABLE_SIZE / 2 - 1;
static constexpr int MIN_I12 = -Cbc::RT::LIT_TABLE_SIZE / 2;

using namespace Cbc::Format;

TEST(EmitTest, InitClosureVariants)
{
    Emitter e;
    e.InitClosure(false);
    e.InitClosure(true);

    auto code = e.Build(heap);
    ASSERT_EQ(2, code.bytecodeSize);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE), code.bytecode[0]);
    EXPECT_EQ(static_cast<uint8_t>(RT::Opcode::INITCLOSURE_SRET), code.bytecode[1]);

    Stream::StringBuffer stream;
    RT::Log(code, stream);
    EXPECT_EQ("0x000: init.closure\n0x001: init.closure.sret\n", stream.ToString());
}

TEST(EmitTest, NewObjGenericVariants)
{
    Emitter e;
    e.NewObjGeneric(IReg::IR2);
    e.NewObjGeneric(IReg::IR3, true);

    auto code = e.Build(heap);
    ASSERT_EQ(4, code.bytecodeSize);

    Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    auto ordinary = RT::B2rr::Decode(reader);
    EXPECT_EQ(RT::Opcode::NEWOBJ_G, ordinary.opc);
    EXPECT_EQ(IReg::IR2, ordinary.rr.x.IR());
    EXPECT_EQ(IReg::IR2, ordinary.rr.y.IR());

    auto pinned = RT::B2rr::Decode(reader);
    EXPECT_EQ(RT::Opcode::NEWOBJ_PINNED_G, pinned.opc);
    EXPECT_EQ(IReg::IR3, pinned.rr.x.IR());
    EXPECT_EQ(IReg::IR3, pinned.rr.y.IR());

    Stream::StringBuffer stream;
    RT::Log(code, stream);
    EXPECT_EQ("0x000: newobj.g IR2\n0x002: newobj.pinned.g IR3\n", stream.ToString());
}

TEST(EmitTest, LastFourBitBuiltinBoxUsesCompactEncoding)
{
    Emitter e;

    e.NewBox(Interpretation::BUILTIN_RUNE);
    auto code = e.Build(heap);

    ASSERT_EQ(code.bytecodeSize, 2u);
    Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    auto instruction = RT::B2xr::Decode(reader);
    EXPECT_EQ(instruction.opc, RT::Opcode::NEWBOX);
    EXPECT_EQ(instruction.xr.imm, Interpretation::BUILTIN_RUNE);
}

TEST(EmitTest, Simple_ArithB2rr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_Mov)
{
    Emitter e;
    e.MovImm(Width::W64, IReg::IR2, 0x7);
    e.Mov(IReg::IR1, IReg::IR2);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(5, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u64, 0x7);
}

TEST(EmitTest, Simple_MovF2I)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12345.678);
    e.Mov(IReg::IR1, FReg::FR0);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(9, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = Interpret(code, U32(0), U32(0));
    EXPECT_EQ(res.u32, 0x4640e6b6);
}

TEST(EmitTest, Simple_FMovI32)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 0.5);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(7, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 0.5);
}

TEST(EmitTest, Simple_FMovI64)
{
    Emitter e;
    e.FMovI64(FReg::FR10, 0.25);
    e.Mov(FReg::FR0, FReg::FR10);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(13, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f64, 0.25);
}

TEST(EmitTest, Simple_ArithB3xrrr)
{
    Emitter e;
    e.Add(Width::W32, IReg::IR1, IReg::IR2, IReg::IR1);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

TEST(EmitTest, Simple_ArithB4xi12rr)
{
    Emitter e;
    e.AddI(Width::W32, IReg::IR1, IReg::IR2, 0xff);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 0xff00); // through literal
    e.SubI(Width::W32, IReg::IR1, IReg::IR1, 0xfff);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(13, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = Interpret(code, U32(0), U32(1));
    EXPECT_EQ(res.u32, 0xf001);
}

TEST(EmitTest, Simple_ArithFP)
{
    Emitter e;
    e.FMovI32(FReg::FR0, 12.5);
    e.FMovI32(FReg::FR1, 2.5);
    e.FMovI32(FReg::FR2, 1.25);
    e.FMovI32(FReg::FR3, 0.25);
    e.Add(Width::W32, FReg::FR0, FReg::FR0, FReg::FR1);
    e.Sub(Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Div(Width::W32, FReg::FR0, FReg::FR0, FReg::FR2);
    e.Mul(Width::W32, FReg::FR0, FReg::FR0, FReg::FR3);
    e.Ret();
    auto code = e.Build(heap);
    EXPECT_EQ(6 * 4 + 3 * 4 + 1, code.bytecodeSize);

    Cbc::RT::Log(code, Stream::Disasm::rt);
    auto res = InterpretFPRes(code, F32(0), F32(0));
    EXPECT_EQ(res.f32, 2.75);
}

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

TEST(EmitTest, Simple_Bcc)
{
    Emitter e;
    auto label = e.NewLabel();
    e.Bcc(CC::EQ, Width::W32, IReg::IR1, IReg::IR2, label);
    e.Add(Width::W32, IReg::IR1, IReg::IR1, IReg::IR2);
    e.Bind(label);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U32(2), U32(2));
    EXPECT_EQ(res.u32, 2);

    auto res2 = Interpret(code, U32(2), U32(1));
    EXPECT_EQ(res2.u32, 3);
}

TEST(EmitTest, Simple_Bcc_Loop)
{
    Emitter e;
    auto loop = e.NewLabel();
    e.Bind(loop);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.Bcc(CC::LT, Width::W32, IReg::IR1, IReg::IR2, loop);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U32(0), U32(100));
    EXPECT_EQ(res.u32, 100);
}

TEST(EmitTest, Simple_Jmp)
{
    Emitter e;
    auto l1 = e.NewLabel();
    auto l2 = e.NewLabel();
    auto l3 = e.NewLabel();
    e.Jmp(l2);
    e.MovImm(Width::W32, IReg::IR1, 1);
    e.Bind(l1);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 10);
    e.Jmp(l3);
    e.MovImm(Width::W32, IReg::IR1, 2);
    e.Bind(l2);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 20);
    e.Jmp(l1);
    e.MovImm(Width::W32, IReg::IR1, 3);
    e.Bind(l3);
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 30);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U32(100), U32(0));
    EXPECT_EQ(res.u32, 160);
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
    e.BccImm(CC::LT, Width::W32, IReg::IR1, 100, loop);    // lit neg offset
    e.BccImm(CC::LT, Width::W32, IReg::IR1, -0xffff, end); // lit neg value, false res
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 1);
    e.BccImm(CC::GE, Width::W32, IReg::IR1, -5, end); // neg value
    e.Ret();
    e.AddI(Width::W32, IReg::IR1, IReg::IR1, 2);
    e.Bind(end);
    e.Ret();

    auto code = e.Build(heap);

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ(res.u32, 101);
}

TEST(EmitTest, Simple_Neg)
{
    Emitter e;
    e.Neg(Width::W32, IReg::IR1, IReg::IR2);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U32(0), U32(10));
    EXPECT_EQ((int32_t)res.u32, -10);
}

TEST(EmitTest, Simple_FNeg)
{
    Emitter e;
    e.Neg(Width::W32, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    float arg1 = 1.2;
    auto res1  = InterpretFPRes(code, F32(0), F32(arg1));
    EXPECT_EQ(res1.f32, -arg1);

    float arg2 = +0.0;
    auto res2  = InterpretFPRes(code, F32(0), F32(arg2));
    EXPECT_EQ(res2.f32, -arg2);
}

TEST(EmitTest, Simple_FSqrt)
{
    Emitter e;
    e.Sqrt(Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    double arg = 16.81;
    auto res   = InterpretFPRes(code, F64(0), F64(arg));
    EXPECT_EQ(res.f64, std::sqrt(arg));
}

TEST(EmitTest, Simple_FAbs)
{
    Emitter e;
    e.Abs(Width::W64, FReg::FR0, FReg::FR1);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    double arg1 = -1.2;
    auto res1   = InterpretFPRes(code, F64(0), F64(arg1));
    EXPECT_EQ(res1.f64, std::fabs(arg1));

    double arg2 = -0.0;
    auto res2   = InterpretFPRes(code, F64(0), F64(arg2));
    EXPECT_EQ(res2.f64, +0.0);
}

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

TEST(EmitTest, Simple_Convert)
{
    Emitter e;
    e.Convert(ConvertType::U8, ConvertType::U32, IReg::IR1, IReg::IR1);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U32(32896), U32(0), F32(0), F32(0));
    EXPECT_EQ(res.u64, U64(128).u64);
}

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
