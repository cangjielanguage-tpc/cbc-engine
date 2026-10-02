#include <gtest/gtest.h>

#include "cbc/abi.h"
#include "cbc/move_resolver.h"
#include "regs_and_slots.h"

using namespace Cbc;

namespace {

struct TestArg {
    bool isFloat;
    bool isRecord;
    bool isReference;
};

struct TestArgTraits {
    static bool IsFloat(const TestArg& a) { return a.isFloat; }
    static bool IsRecord(const TestArg& a) { return a.isRecord; }
    static bool IsReference(const TestArg& a) { return a.isReference; }
};

Location Ir(int idx) { return Location::IReg(static_cast<IReg::Value>(idx)); }
Location Fr(int idx) { return Location::FReg(static_cast<FReg::Value>(idx)); }

TestArg IntArg() { return {false, false, false}; }
TestArg FloatArg() { return {true, false, false}; }

} // namespace

static auto X64Desc = AbiBuilder::PlatformDescription::FromTraits<Platform::LINUX_X64>();
static auto A64Desc = AbiBuilder::PlatformDescription::FromTraits<Platform::LINUX_AARCH64>();

TEST(AbiBuilder, NoFlagsIntParams)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Ir(3), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Ir(4), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Ir(5), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(3));
    EXPECT_EQ(r.iregs[2], IrValue(4));
    EXPECT_EQ(r.iregs[3], IrValue(5));
}

TEST(AbiBuilder, NoFlagsFloatParams)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Fr(3), { .isFloat = true, .isRecord = false, .isReference = false });
    builder.Consume(Fr(4), { .isFloat = true, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.fregs[0], FrValue(3));
    EXPECT_EQ(r.fregs[1], FrValue(4));
}

TEST(AbiBuilder, MixedIntFloatParams)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Ir(7), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Fr(2), { .isFloat = true, .isRecord = false, .isReference = false });
    builder.Consume(Ir(8), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Fr(3), { .isFloat = true, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.fregs[0], FrValue(2));
    EXPECT_EQ(r.fregs[1], FrValue(3));
}

TEST(AbiBuilder, SRetX86Shifts)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.ConsumeSret(Ir(7));
    builder.Consume(Ir(8), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, SRetAarch64Shifts)
{
    MoveResolver mr;
    AbiBuilder builder {mr, A64Desc};

    builder.ConsumeSret(Ir(7));
    builder.Consume(Ir(8), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, MutReceiver)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.ConsumeReceiverMut(Ir(7), Ir(8));
    builder.Consume(Ir(9), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.iregs[3], IrValue(9));
}

TEST(AbiBuilder, RefReceiver)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.ConsumeReceiver(Ir(7));
    builder.Consume(Ir(8), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, RecReceiver)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.ConsumeReceiver(Ir(7));
    builder.Consume(Ir(8), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, IntOverflowToStack)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    for (int i = 7; i <= 13; i++) {
        builder.Consume(Ir(i), { .isFloat = false, .isRecord = false, .isReference = false });
    }

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 7);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.iregs[3], IrValue(9));
    EXPECT_EQ(r.iregs[4], IrValue(10));
    EXPECT_EQ(r.iregs[5], IrValue(11));
    EXPECT_EQ(r.iregs[6], IrValue(12));
    EXPECT_EQ(r.paramPassingStackSlots[0], IrValue(13));
}

TEST(AbiBuilder, FloatParamsNoOverflow)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    for (int i = 8; i < 16; i++) {
        builder.Consume(Fr(i), { .isFloat = true, .isRecord = false, .isReference = false });
    }

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 8);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(r.fregs[i], FrValue(i + 8));
    }
}

TEST(AbiBuilder, InterfaceCallGeneric)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Ir(7), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.ConsumeFtvars(Ir(8));

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, SRetPlusMutPlusParams)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.ConsumeSret(Ir(7));
    builder.ConsumeReceiverMut(Ir(8), Ir(9));
    builder.Consume(Ir(10), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Ir(11), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 5);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.iregs[3], IrValue(9));
    EXPECT_EQ(r.iregs[4], IrValue(10));
    EXPECT_EQ(r.iregs[5], IrValue(11));
}

TEST(AbiBuilder, Aarch64MoreIntRegs)
{
    MoveResolver mr;
    AbiBuilder builder {mr, A64Desc};

    for (int i = 6; i <= 13; i++) {
        builder.Consume(Ir(i), { .isFloat = false, .isRecord = false, .isReference = false });
    }

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 8);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(r.iregs[i + 1], IrValue(i + 6));
    }
}

TEST(AbiBuilder, SelfMoveProducesNoEmit)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Ir(1), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    EXPECT_EQ(r.movCount, 0);
}

TEST(AbiBuilder, CycleInArgs)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume(Ir(2), { .isFloat = false, .isRecord = false, .isReference = false });
    builder.Consume(Ir(1), { .isFloat = false, .isRecord = false, .isReference = false });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.iregs[2], IrValue(1));
}

TEST(AbiBuilder, PlatformParameterized)
{
    // x86_64: 6 int regs, 2 overflow to stack
    {
        MoveResolver mr;
        AbiBuilder builder {mr, X64Desc};
        for (int i = 6; i <= 13; i++) {
            builder.Consume(Ir(i), { .isFloat = false, .isRecord = false, .isReference = false });
        }
        RegsAndSlots r;
        r.Resolve(mr);
        ASSERT_EQ(r.movCount, 8);
        EXPECT_EQ(r.iregs[6], IrValue(11));
        EXPECT_EQ(r.paramPassingStackSlots[0], IrValue(12));
        EXPECT_EQ(r.paramPassingStackSlots[1], IrValue(13));
    }

    // aarch64: 8 int regs, no overflow
    {
        MoveResolver mr;
        AbiBuilder builder {mr, A64Desc};
        for (int i = 6; i <= 13; i++) {
            builder.Consume(Ir(i), { .isFloat = false, .isRecord = false, .isReference = false });
        }
        RegsAndSlots r;
        r.Resolve(mr);
        ASSERT_EQ(r.movCount, 8);
        EXPECT_EQ(r.iregs[8], IrValue(13));
    }
}

TEST(AbiBuilder, TemplateAdapter)
{
    MoveResolver mr;
    AbiBuilder builder {mr, X64Desc};

    builder.Consume<TestArg, TestArgTraits>(IntArg(), Ir(7));
    builder.Consume<TestArg, TestArgTraits>(FloatArg(), Fr(3));
    builder.Consume<TestArg, TestArgTraits>(IntArg(), Ir(8));

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.fregs[0], FrValue(3));
}
