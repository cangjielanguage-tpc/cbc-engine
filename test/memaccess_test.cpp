#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/isa_rt.h"

#include "mock/interpreter.h"
#include "testutils.h"

#define UNIT_TEST_MODE 1

static LimitedHeap<16384> heap;

class EmitTest : public testing::Test {
    void SetUp() override {
        heap.Reset();
    }

    void TearDown() override { }
};


using namespace Cbc::Emitter;
using namespace Cbc::Format;
using namespace Cbc;

/// Allocate type info that describes object of size `objectSize` (including header).
static TestTypeInfo* NewTypeInfo(size_t objectSize) {
    void* mem = heap.do_allocate(sizeof(TestTypeInfo), alignof(TestTypeInfo));
    return new (mem) TestTypeInfo{objectSize};
}

TEST(MemoryAccess, TestAlloc) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR1, sym);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR2, IReg::IR1, 8);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR1, IReg::IR1, 8);
    e.Ret();
    auto res = Interpret(e.Build(heap), U32(0), U64(2));
    EXPECT_EQ(res.u32, 2);
}

TEST(MemoryAccess, TestAlloc2) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(24);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR2, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR1, IReg::IR5, 16);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR10, IReg::IR5, 16);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(77), U64(91));
    EXPECT_EQ(res.u64, 77 + 91);
}

TEST(MemoryAccess, TestU8) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR1, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR2, IReg::IR5, 9);
    e.LoadObj(Format::LoadAccessKind::LD_U8, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_U8, IReg::IR10, IReg::IR5, 9);
    e.Add(Width::W32, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(77), U64(91));
    EXPECT_EQ(res.u32, 77 + 91);
}

TEST(MemoryAccess, TestI8) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR1, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_8, IReg::IR2, IReg::IR5, 9);
    e.LoadObj(Format::LoadAccessKind::LD_S8, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S8, IReg::IR10, IReg::IR5, 9);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(128), U64(2));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, -128 + 2);
}

TEST(MemoryAccess, TestI16) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_16, IReg::IR1, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S16, IReg::IR1, IReg::IR5, 8);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(0xffff), U64(0));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, -1);
}

TEST(MemoryAccess, TestI32) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR1, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR2, IReg::IR5, 12);
    e.LoadObj(Format::LoadAccessKind::LD_S32TO64, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_S32TO64, IReg::IR10, IReg::IR5, 12);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(2147483647L + 1), U64(9));
    auto i64 = static_cast<int64_t>(res.u64);
    EXPECT_EQ(i64, INT32_MIN + 9);
}

TEST(MemoryAccess, TestU32) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(16);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR1, IReg::IR5, 8);
    e.StoreObj(Format::StoreAccessKind::ST_32, IReg::IR2, IReg::IR5, 12);
    e.LoadObj(Format::LoadAccessKind::LD_32, IReg::IR9, IReg::IR5, 8);
    e.LoadObj(Format::LoadAccessKind::LD_32, IReg::IR10, IReg::IR5, 12);
    e.Add(Width::W64, IReg::IR1, IReg::IR9, IReg::IR10);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(2147483647L + 1), U64(9));
    EXPECT_EQ(res.u64, 2147483647L + 1 + 9);
}

TEST(MemoryAccess, LinkedStack) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(24);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    auto fillStack = e.NewLabel();
    auto dropStack = e.NewLabel();

    e.MovImm(Width::W32, IReg::IR11, 10);
    e.Mov(IReg::IR10, IReg::IR11);

    e.MovRef(IReg::IR2, IReg::IRZ);
    e.Bind(fillStack);
    e.NewObj(IReg::IR1, sym);
    e.StoreObj(Format::StoreAccessKind::ST_REF, IReg::IR2, IReg::IR1, 8);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR10, IReg::IR1, 16);
    e.MovRef(IReg::IR2, IReg::IR1);
    e.AddI(Width::W32, IReg::IR10, IReg::IR10, static_cast<uint64_t>(-1));
    e.Bcc(CC::LT, Width::W32, IReg::IRZ, IReg::IR10, fillStack);

    e.Mov(IReg::IR10, IReg::IR11);
    e.Mov(IReg::IR4, IReg::IRZ);

    e.Bind(dropStack);

    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR3, IReg::IR2, 16);
    e.Add(Width::W32, IReg::IR4, IReg::IR4, IReg::IR3);

    e.LoadObj(Format::LoadAccessKind::LD_REF, IReg::IR1, IReg::IR2, 8);
    e.MovRef(IReg::IR2, IReg::IR1);

    e.AddI(Width::W32, IReg::IR10, IReg::IR10, static_cast<uint64_t>(-2));
    e.Bcc(CC::LT, Width::W32, IReg::IRZ, IReg::IR10, dropStack);

    e.Mov(IReg::IR1, IReg::IR4);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(77), U64(91));
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

static void testInteger(IntegerTest desc) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(512);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.MovImm(Width::W64, IReg::IR4, 2);
    e.NewObj(IReg::IR5, sym);

    auto mspace = e.OpenMemSpace();
    mspace.Offset(31); // = 31
    mspace.Offset(47); // = 78
    mspace.OffsetReg(IReg::IR4); // + 2 = 80
    mspace.StoreObj(desc.stk, IReg::IR2, IReg::IR5);
    e.StoreObj(desc.stk, IReg::IR1, IReg::IR5, 80 + desc.size);

    auto mspace2 = e.OpenMemSpace();
    mspace.Offset(31); // = 31
    mspace.Offset(47); // = 78
    mspace.OffsetReg(IReg::IR4); // + 2 = 80
    mspace.LoadObj(desc.ldk, IReg::IR1, IReg::IR5);
    e.LoadObj(desc.ldk, IReg::IR2, IReg::IR5, 80 + desc.size);

    e.Mov(IReg::IR3, IReg::IR1);
    e.Mov(IReg::IR4, IReg::IR2);
    e.Div(Width::W64, IReg::IR1, IReg::IR3, IReg::IR4);
    e.Ret();

    auto res = Interpret(e.Build(heap), U64(desc.ir1), U64(desc.ir2));
    EXPECT_EQ(res.u64, desc.expect);
}

TEST(MemoryAccess, SpaceS8) {
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_S8,
        .stk = Format::StoreAccessKind::ST_8,
        .size = 1,
        .ir1 = static_cast<uint64_t>(-3),
        .ir2 = static_cast<uint64_t>(-9),
        .expect = 3
    });
}

TEST(MemoryAccess, SpaceS16) {
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_S16,
        .stk = Format::StoreAccessKind::ST_16,
        .size = 2,
        .ir1 = static_cast<uint64_t>(-3000),
        .ir2 = static_cast<uint64_t>(-12000),
        .expect = 4
    });
}

TEST(MemoryAccess, SpaceS32) {
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_S32TO64,
        .stk = Format::StoreAccessKind::ST_32,
        .size = 4,
        .ir1 = static_cast<uint64_t>(-30000000),
        .ir2 = static_cast<uint64_t>(-60000000),
        .expect = 2
    });
}

TEST(MemoryAccess, SpaceU8) {
    auto lhs = static_cast<uint64_t>(-6);
    auto rhs = static_cast<uint64_t>(-3);
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_U8,
        .stk = Format::StoreAccessKind::ST_8,
        .size = 8,
        .ir1 = rhs,
        .ir2 = lhs,
        .expect = lhs / rhs
    });
}

TEST(MemoryAccess, Space64) {
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_64,
        .stk = Format::StoreAccessKind::ST_64,
        .size = 8,
        .ir1 = static_cast<uint64_t>(-3000000000000000l),
        .ir2 = static_cast<uint64_t>(-6000000000000000l),
        .expect = 2
    });
}

TEST(MemoryAccess, SpaceU32) {
    uint64_t lhs = static_cast<uint32_t>(-300000000);
    auto rhs = static_cast<uint64_t>(40);
    testInteger(IntegerTest {
        .ldk = Format::LoadAccessKind::LD_32,
        .stk = Format::StoreAccessKind::ST_32,
        .size = 8,
        .ir1 = rhs,
        .ir2 = lhs,
        .expect = lhs / rhs
    });
}

TEST(MemoryAccess, Fallback) {
    Cbc::Emitter::Emitter e;
    auto ti = NewTypeInfo(5000);
    auto sym = e.NewAddressSym(reinterpret_cast<uintptr_t>(ti));
    e.NewObj(IReg::IR5, sym);
    e.StoreObj(Format::StoreAccessKind::ST_64, IReg::IR2, IReg::IR5, 4096);
    e.LoadObj(Format::LoadAccessKind::LD_64, IReg::IR1, IReg::IR5, 4096);
    e.Ret();

    auto code = e.Build(heap);
    auto expected = 1231230123;
    auto res = Interpret(code, U64(16), U64(expected));
    EXPECT_EQ(res.u64, expected);
}

