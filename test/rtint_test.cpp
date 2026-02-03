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

    void TearDown() override {

    }
};


using namespace Cbc::Emitter;
using namespace Cbc::Format;
using namespace Cbc;

/// Allocate type info that describes object of size `objectSize` (including header).
static TestTypeInfo* NewTypeInfo(size_t objectSize) {
    void* mem = heap.do_allocate(sizeof(TestTypeInfo), alignof(TestTypeInfo));
    return new (mem) TestTypeInfo{objectSize};
}

TEST(RTInterfaceTest, TestAlloc) {
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

TEST(RTInterfaceTest, TestAlloc2) {
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
