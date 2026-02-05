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
