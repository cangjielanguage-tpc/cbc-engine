#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/formater_rt.h"
#include "cbc/isa.h"
#include "cbc/isa_rt.h"

#include "mock/interpreter.h"
#include "runtimesupport/runtime.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class MemoryAccess : public testing::Test {
    void SetUp() override
    {
        InitializeMockInterpreter();
        heap.Reset();
    }

    void TearDown() override {}
};

using namespace Cbc::Emitter;
using namespace Cbc::Format;
using namespace Cbc;

/// Allocate type info that describes object of size `objectSize` (including header).
static RTSupport::TypeInfo NewTypeInfo(size_t objectSize)
{
    return RTSupport::TypeInfo(heap.New<TestTypeInfo>(objectSize));
}

TEST_F(MemoryAccess, TestAlloc)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR10, IReg::IR2);
    e.NewObj(ti);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR10, IReg::IR1, 8);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR1, IReg::IR1, 8);
    e.Ret();
    auto res = Interpret(e.Build(heap), U32(0), U64(2));
    EXPECT_EQ(res.u32, 2);
}

TEST_F(MemoryAccess, TestAlloc2)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(24);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR12, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR11, IReg::IR5, 16);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR10, IReg::IR5, 16);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U64(77), U64(91));
    EXPECT_EQ(res.u64, 77 + 91);
}

TEST_F(MemoryAccess, TestU8)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR11, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR12, IReg::IR5, 9);
    e.LoadObj(Format::LoadAccessKind::LD_U8, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_U8, IReg::IR10, IReg::IR5, 9);
    e.Add(Width::W32, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(77), U64(91));
    EXPECT_EQ(res.u32, 77 + 91);
}

TEST_F(MemoryAccess, TestI8)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR11, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR12, IReg::IR5, 9);
    e.LoadObj(Format::LoadAccessKind::LD_S8, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S8, IReg::IR10, IReg::IR5, 9);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(128), U64(2));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, -128 + 2);
}

TEST_F(MemoryAccess, TestI16)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR11, IReg::IR1);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_16, IReg::IR11, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S16, IReg::IR1, IReg::IR5, 8);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(0xffff), U64(0));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, -1);
}

TEST_F(MemoryAccess, TestI32)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR11, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR12, IReg::IR5, 12);
    e.LoadObj(Format::LoadAccessKind::LD_S32TO64, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S32TO64, IReg::IR10, IReg::IR5, 12);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(2147483647L + 1), U64(9));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, INT32_MIN + 9);
}

TEST_F(MemoryAccess, TestU32)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR11, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR12, IReg::IR5, 12);
    e.LoadObj(Format::LoadAccessKind::LD_32, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_32, IReg::IR10, IReg::IR5, 12);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(2147483647L + 1), U64(9));
    EXPECT_EQ(res.u64, 2147483647L + 1 + 9);
}

TEST_F(MemoryAccess, TestF32)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.FMovI32(FReg::FR0, 0.25);
    e.StoreObj(Format::StoreAccessKind::ST_F32, FReg::FR0, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_F32, FReg::FR3, IReg::IR5, 8);
    e.Add(Width::W32, FReg::FR0, FReg::FR0, FReg::FR3);
    e.Ret();

    auto res = InterpretFPRes(e.Build(heap), U64(1), U64(2));
    EXPECT_EQ(res.f32, 0.5);
}

TEST_F(MemoryAccess, TestF64)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.FMovI64(FReg::FR0, 0.25);
    e.StoreObj(Format::StoreAccessKind::ST_F64, FReg::FR0, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_F64, FReg::FR3, IReg::IR5, 8);
    e.Add(Width::W64, FReg::FR0, FReg::FR0, FReg::FR3);
    e.Ret();

    auto res = InterpretFPRes(e.Build(heap), U64(1), U64(2));
    EXPECT_EQ(res.f64, 0.5);
}

TEST_F(MemoryAccess, LinkedStack)
{
    Cbc::Emitter::Emitter e;
    auto ti        = NewTypeInfo(24);
    auto fillStack = e.NewLabel();
    auto dropStack = e.NewLabel();

    e.MovImm(Width::W32, IReg::IR11, 10);
    e.Mov(IReg::IR10, IReg::IR11);

    e.Mov(IReg::IR12, IReg::IRZ);
    e.Bind(fillStack);
    e.NewObj(ti);
    e.StoreObj(Format::StoreAccessKind::ST_REF, IReg::IR12, IReg::IR1, 8);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR10, IReg::IR1, 16);
    e.Mov(IReg::IR12, IReg::IR1);
    e.AddI(Width::W32, IReg::IR10, IReg::IR10, static_cast<uint64_t>(-1));
    e.Bcc(CC::LT, Width::W32, IReg::IRZ, IReg::IR10, fillStack);

    e.Mov(IReg::IR10, IReg::IR11);
    e.Mov(IReg::IR4, IReg::IRZ);

    e.Bind(dropStack);

    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR3, IReg::IR12, 16);
    e.Add(Width::W32, IReg::IR4, IReg::IR4, IReg::IR3);

    e.LoadObj(Format::LoadAccessKind::LD_REF, IReg::IR1, IReg::IR12, 8);
    e.Mov(IReg::IR12, IReg::IR1);

    e.AddI(Width::W32, IReg::IR10, IReg::IR10, static_cast<uint64_t>(-2));
    e.Bcc(CC::LT, Width::W32, IReg::IRZ, IReg::IR10, dropStack);

    e.Mov(IReg::IR1, IReg::IR4);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U64(77), U64(91));
    EXPECT_EQ(res.u32, 1 + 2 + 3 + 4 + 5);
}

struct IntegerTest {
    Format::LoadAccessKind ldk;
    Format::StoreAccessKind stk;
    int size;
    uint64_t ir1;
    uint64_t ir2;
    uint64_t expect;
};

static void testInteger(IntegerTest desc)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(512);
    e.MovImm(Width::W64, IReg::IR4, 2);
    e.Mov(IReg::IR11, IReg::IR1);
    e.Mov(IReg::IR12, IReg::IR2);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);

    auto mspace = e.OpenMemSpace();
    mspace.Offset(31);           // = 31
    mspace.Offset(47);           // = 78
    mspace.OffsetReg(IReg::IR4); // + 2 = 80
    mspace.StoreObj(desc.stk, IReg::IR2, IReg::IR5);
    e.StoreObj(desc.stk, IReg::IR11, IReg::IR5, 80 + desc.size);

    auto mspace2 = e.OpenMemSpace();
    mspace.Offset(31);           // = 31
    mspace.Offset(47);           // = 78
    mspace.OffsetReg(IReg::IR4); // + 2 = 80
    mspace.LoadObj(desc.ldk, IReg::IR11, IReg::IR5);
    e.LoadObj(desc.ldk, IReg::IR12, IReg::IR5, 80 + desc.size);

    e.Mov(IReg::IR3, IReg::IR11);
    e.Mov(IReg::IR4, IReg::IR12);
    e.Div(Width::W64, IReg::IR11, IReg::IR3, IReg::IR4);

    e.Mov(IReg::IR1, IReg::IR11);
    e.Mov(IReg::IR2, IReg::IR12);
    e.Ret();

    auto code = e.Build(heap);
    Cbc::RT::Log(code, Stream::Disasm::rt);

    auto res = Interpret(code, U64(desc.ir1), U64(desc.ir2));
    EXPECT_EQ(res.u64, desc.expect);
}

TEST_F(MemoryAccess, SpaceS8)
{
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_S8,
                              .stk    = Format::StoreAccessKind::ST_8,
                              .size   = 1,
                              .ir1    = static_cast<uint64_t>(-3),
                              .ir2    = static_cast<uint64_t>(-9),
                              .expect = 3 });
}

TEST_F(MemoryAccess, SpaceS16)
{
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_S16,
                              .stk    = Format::StoreAccessKind::ST_16,
                              .size   = 2,
                              .ir1    = static_cast<uint64_t>(-3000),
                              .ir2    = static_cast<uint64_t>(-12000),
                              .expect = 4 });
}

TEST_F(MemoryAccess, SpaceS32)
{
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_S32TO64,
                              .stk    = Format::StoreAccessKind::ST_32,
                              .size   = 4,
                              .ir1    = static_cast<uint64_t>(-30000000),
                              .ir2    = static_cast<uint64_t>(-60000000),
                              .expect = 2 });
}

TEST_F(MemoryAccess, SpaceU8)
{
    auto lhs = static_cast<uint64_t>(-6);
    auto rhs = static_cast<uint64_t>(-3);
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_U8,
                              .stk    = Format::StoreAccessKind::ST_8,
                              .size   = 8,
                              .ir1    = rhs,
                              .ir2    = lhs,
                              .expect = lhs / rhs });
}

TEST_F(MemoryAccess, Space64)
{
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_64,
                              .stk    = Format::StoreAccessKind::ST_64,
                              .size   = 8,
                              .ir1    = static_cast<uint64_t>(-3000000000000000l),
                              .ir2    = static_cast<uint64_t>(-6000000000000000l),
                              .expect = 2 });
}

TEST_F(MemoryAccess, SpaceU32)
{
    uint64_t lhs = static_cast<uint32_t>(-300000000);
    auto rhs     = static_cast<uint64_t>(40);
    testInteger(IntegerTest { .ldk    = Format::LoadAccessKind::LD_32,
                              .stk    = Format::StoreAccessKind::ST_32,
                              .size   = 8,
                              .ir1    = rhs,
                              .ir2    = lhs,
                              .expect = lhs / rhs });
}

TEST_F(MemoryAccess, Fallback)
{
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(5000);
    e.NewObj(ti);
    e.Mov(IReg::IR5, IReg::IR1);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR2, IReg::IR5, 4096);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR1, IReg::IR5, 4096);
    e.Ret();

    auto code     = e.Build(heap);
    auto expected = 1231230123;
    auto res      = Interpret(code, U64(16), U64(expected));
    EXPECT_EQ(res.u64, expected);
}

struct StructTest {
    uint64_t u64;
    uint32_t u32;
};

TEST_F(MemoryAccess, TestStructSpace)
{
    Cbc::Emitter::Emitter e;
    StructTest structTest { .u64 = 0, .u32 = 0 };

    e.MovImm(Width::W64, IReg::IR4, 42);
    auto mspace1 = e.OpenMemSpace();
    mspace1.StoreRec(Format::StoreAccessKind::ST_64, IReg::IR4, IReg::IR2);

    e.MovImm(Width::W32, IReg::IR5, 34);
    auto mspace2 = e.OpenMemSpace();
    mspace2.Offset(sizeof(uint64_t));
    mspace2.StoreRec(Format::StoreAccessKind::ST_32, IReg::IR5, IReg::IR2);

    auto mspace3 = e.OpenMemSpace();
    mspace3.LoadRec(Format::LoadAccessKind::LD_64, IReg::IR6, IReg::IR2);

    auto mspace4 = e.OpenMemSpace();
    mspace2.Offset(sizeof(uint64_t));
    mspace4.LoadRec(Format::LoadAccessKind::LD_32, IReg::IR7, IReg::IR2);

    e.Add(Width::W64, IReg::IR1, IReg::IR6, IReg::IR7);
    e.Ret();

    auto res = Interpret(e.Build(heap), U32(0), U64(reinterpret_cast<uint64_t>(&structTest)));
    EXPECT_EQ(structTest.u64, 42);
    EXPECT_EQ(structTest.u32, 34);
    EXPECT_EQ(res.u64, 76);
}

TEST_F(MemoryAccess, TestStruct)
{
    Cbc::Emitter::Emitter e;
    StructTest structTest { .u64 = 0, .u32 = 0 };

    e.MovImm(Width::W64, IReg::IR4, 42);
    e.StoreRec(Format::StoreAccessKind::ST_64, IReg::IR4, IReg::IR2, 0);
    e.LoadRec(Format::LoadAccessKind::LD_64, IReg::IR6, IReg::IR2, 0);

    e.MovImm(Width::W32, IReg::IR5, 34);
    e.StoreRec(Format::StoreAccessKind::ST_32, IReg::IR5, IReg::IR2, sizeof(uint64_t));
    e.LoadRec(Format::LoadAccessKind::LD_32, IReg::IR7, IReg::IR2, sizeof(uint64_t));

    e.Add(Width::W64, IReg::IR1, IReg::IR6, IReg::IR7);
    e.Ret();

    auto res = Interpret(e.Build(heap), U32(0), U64(reinterpret_cast<uint64_t>(&structTest)));
    EXPECT_EQ(structTest.u64, 42);
    EXPECT_EQ(structTest.u32, 34);
    EXPECT_EQ(res.u64, 76);
}

TEST_F(MemoryAccess, TestFrameSpace)
{
    char frameSlots[32];
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
    Interpretation::Frame frame { frameStart };

    Cbc::Emitter::Emitter e;

    e.MovImm(Width::W64, IReg::IR4, 42);
    auto mspace1 = e.OpenMemSpace();
    mspace1.StoreFrame(Format::StoreAccessKind::ST_64, IReg::IR4);

    e.MovImm(Width::W32, IReg::IR5, 34);
    auto mspace2 = e.OpenMemSpace();
    mspace2.Offset(16);
    mspace2.StoreFrame(Format::StoreAccessKind::ST_32, IReg::IR5);

    auto mspace3 = e.OpenMemSpace();
    mspace3.LoadFrame(Format::LoadAccessKind::LD_64, IReg::IR6);

    auto mspace4 = e.OpenMemSpace();
    mspace2.Offset(16);
    mspace4.LoadFrame(Format::LoadAccessKind::LD_32, IReg::IR7);

    e.Add(Width::W64, IReg::IR1, IReg::IR6, IReg::IR7);
    e.Ret();

    auto res = Interpret(e.Build(heap), frame, U32(0), U64(0));
    EXPECT_EQ(*reinterpret_cast<long*>(frameSlots), 42);
    EXPECT_EQ(*reinterpret_cast<int*>(frameSlots + 16), 34);
    EXPECT_EQ(res.u64, 76);
}

TEST_F(MemoryAccess, TestFrame)
{
    char frameSlots[32];
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
    Interpretation::Frame frame { frameStart };

    Cbc::Emitter::Emitter e;

    e.MovImm(Width::W64, IReg::IR4, 42);
    e.StoreFrame(Format::StoreAccessKind::ST_64, IReg::IR4, 0);
    e.LoadFrame(Format::LoadAccessKind::LD_64, IReg::IR6, 0);

    e.MovImm(Width::W32, IReg::IR5, 34);
    e.StoreFrame(Format::StoreAccessKind::ST_32, IReg::IR5, 16);
    e.LoadFrame(Format::LoadAccessKind::LD_32, IReg::IR7, 16);

    e.Add(Width::W64, IReg::IR1, IReg::IR6, IReg::IR7);
    e.Ret();

    auto res = Interpret(e.Build(heap), frame, U32(0), U64(0));
    EXPECT_EQ(*reinterpret_cast<long*>(frameSlots), 42);
    EXPECT_EQ(*reinterpret_cast<int*>(frameSlots + 16), 34);
    EXPECT_EQ(res.u64, 76);
}

TEST_F(MemoryAccess, TestFrameImm)
{
    uint64_t frameSlots[10];
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
    Interpretation::Frame frame { frameStart };

    Cbc::Emitter::Emitter e;

    e.StoreFrameImm(Format::StoreAccessKind::ST_8, 1, 0);
    e.StoreFrameImm(Format::StoreAccessKind::ST_8, 2, 1);
    e.StoreFrameImm(Format::StoreAccessKind::ST_16, 3, 2);
    e.StoreFrameImm(Format::StoreAccessKind::ST_32, 4, 4);
    e.StoreFrameImm(Format::StoreAccessKind::ST_64, 8, 8);

    e.StoreFrameImm(Format::StoreAccessKind::ST_16, 0xfffffffffffff000, 16);
    e.StoreFrameImm(Format::StoreAccessKind::ST_16, 0xfff, 18);
    e.StoreFrameImm(Format::StoreAccessKind::ST_32, 0x1234, 20);
    e.StoreFrameImm(Format::StoreAccessKind::ST_64, 0x4321, 24);

    e.StoreFrameImm(Format::StoreAccessKind::ST_32, 0xfffffffff0000000, 32);
    e.StoreFrameImm(Format::StoreAccessKind::ST_32, 0xfffffff, 36);
    e.StoreFrameImm(Format::StoreAccessKind::ST_64, 0x1fffffff, 40);

    e.StoreFrameImm(Format::StoreAccessKind::ST_64, 0x1fffffffffffffff, 48);

    e.Ret();

    auto code = e.Build(heap);
    auto res  = Interpret(code, frame, U32(0), U64(0));

    EXPECT_EQ(
        code.bytecodeSize, 13 * 4 + 5 * 2 + 4 * 3 + 3 * 5 + 1 * 9 + 1 - 3
    ); // 13x(MemOpen+Offs) + 5xM2i8 + 4xM3i16 + 3xM5i32 + 1xM9i64 + Ret - 1xM3i16

    EXPECT_EQ(frameSlots[0], 0x0000000400030201);
    EXPECT_EQ(frameSlots[1], 0x0000000000000008);
    EXPECT_EQ(frameSlots[2], 0x000012340ffff000);
    EXPECT_EQ(frameSlots[3], 0x0000000000004321);
    EXPECT_EQ(frameSlots[4], 0x0ffffffff0000000);
    EXPECT_EQ(frameSlots[5], 0x000000001fffffff);
    EXPECT_EQ(frameSlots[6], 0x1fffffffffffffff);
}
