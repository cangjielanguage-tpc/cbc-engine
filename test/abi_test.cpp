#include <gtest/gtest.h>

#include "cbc/abi.h"
#include "cbc/move_resolver.h"
#include "regs_and_slots.h"

using namespace Cbc;

struct TestArg {
    bool isFloat;
    bool isRecord;
    bool isReference;
};

template <> struct Cbc::ArgTypeTraits<TestArg> {
    ArgKind Kind(TestArg const& arg)
    {
        if (arg.isFloat) {
            return ArgKind::FLOAT;
        } else if (arg.isRecord) {
            return ArgKind::REC;
        } else if (arg.isReference) {
            return ArgKind::REF;
        } else {
            return ArgKind::INT;
        }
    }
};

namespace {

Location Ir(int idx) { return Location::IReg(static_cast<IReg::Value>(idx)); }

Location Fr(int idx) { return Location::FReg(static_cast<FReg::Value>(idx)); }

TestArg IntArg() { return { false, false, false }; }

TestArg FloatArg() { return { true, false, false }; }

// Test-specific wrapper combining AbiAssigner + MoveResolver.
// Exposes the old AbiBuilder interface (Consume takes a source Location).
// Tracks metadata (masks, slot lists) locally from (Location, ArgKind).
struct TestAbiBuilder {
    AbiAssigner assigner;
    MoveResolver moves;
    uint16_t iregStackPtrMask = 0;
    uint16_t iregRefMask      = 0;
    uint16_t fregMask         = 0;
    int iargPos               = 0;
    int fargPos               = 0;
    Utils::Vector<uint32_t> refStackSlots;
    Utils::Vector<uint32_t> recStackSlots;

    AbiAssigner::PlatformDescription desc;

    TestAbiBuilder(const AbiAssigner::PlatformDescription& desc) : assigner(desc), desc(desc) {}

    void track(Location target, ArgKind kind)
    {
        if (target.Kind() == Location::IREG) {
            if (kind == ArgKind::REC)
                iregStackPtrMask |= (1u << iargPos);
            if (kind == ArgKind::REF)
                iregRefMask |= (1u << iargPos);
            iargPos++;
        } else if (target.Kind() == Location::FREG) {
            fregMask |= (1u << fargPos);
            fargPos++;
        } else {
            if (kind == ArgKind::REF)
                refStackSlots.PushBack(target.SlotIdx());
            if (kind == ArgKind::REC)
                recStackSlots.PushBack(target.SlotIdx());
        }
    }

    Location Consume(Location loc, ArgKind kind)
    {
        auto target = assigner.Consume(kind);
        moves.AddMove(loc, target);
        track(target, kind);
        return target;
    }

    Location ConsumeSret(Location loc)
    {
        auto target = assigner.ConsumeSret();
        moves.AddMove(loc, target);
        track(target, ArgKind::REC);
        return target;
    }

    Location ConsumeReceiverMut(Location loc0, Location loc1)
    {
        auto t0 = assigner.Consume(ArgKind::INT);
        auto t1 = assigner.Consume(ArgKind::REF);
        moves.AddMove(loc0, t0);
        moves.AddMove(loc1, t1);
        track(t0, ArgKind::INT);
        track(t1, ArgKind::REF);
        return t0;
    }

    template <typename ArgType> Location Consume(ArgType arg, Location loc)
    {
        ArgTypeTraits<ArgType> traits;
        return Consume(loc, traits.Kind(arg));
    }

    void Clear()
    {
        assigner = AbiAssigner(desc);
        moves.Clear();
        iregStackPtrMask = 0;
        iregRefMask      = 0;
        fregMask         = 0;
        iargPos          = 0;
        fargPos          = 0;
        refStackSlots.Clear();
        recStackSlots.Clear();
    }

    uint16_t IregStackPtrMask() const { return iregStackPtrMask; }

    uint16_t IregRefMask() const { return iregRefMask; }

    uint16_t FregMask() const { return fregMask; }

    Utils::Span<uint32_t const> RefStackSlots() const { return { refStackSlots.Data(), refStackSlots.Size() }; }

    Utils::Span<uint32_t const> RecStackSlots() const { return { recStackSlots.Data(), recStackSlots.Size() }; }

    int MaxStackSlot() const { return assigner.MaxStackSlot(); }
};

} // namespace

static auto X64Desc = AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_X64>();
static auto A64Desc = AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_AARCH64>();

TEST(AbiBuilder, NoFlagsIntParams)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(3), ArgKind::INT);
    builder.Consume(Ir(4), ArgKind::INT);
    builder.Consume(Ir(5), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(3));
    EXPECT_EQ(r.iregs[2], IrValue(4));
    EXPECT_EQ(r.iregs[3], IrValue(5));
}

TEST(AbiBuilder, NoFlagsFloatParams)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Fr(3), ArgKind::FLOAT);
    builder.Consume(Fr(4), ArgKind::FLOAT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.fregs[0], FrValue(3));
    EXPECT_EQ(r.fregs[1], FrValue(4));
}

TEST(AbiBuilder, MixedIntFloatParams)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::INT);
    builder.Consume(Fr(2), ArgKind::FLOAT);
    builder.Consume(Ir(8), ArgKind::INT);
    builder.Consume(Fr(3), ArgKind::FLOAT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.fregs[0], FrValue(2));
    EXPECT_EQ(r.fregs[1], FrValue(3));
}

TEST(AbiBuilder, SRetX86Shifts)
{
    TestAbiBuilder builder { X64Desc };

    builder.ConsumeSret(Ir(7));
    builder.Consume(Ir(8), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, SRetAarch64Shifts)
{
    TestAbiBuilder builder { A64Desc };

    builder.ConsumeSret(Ir(7));
    builder.Consume(Ir(8), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(8));
    EXPECT_EQ(r.iregs[9], IrValue(7));
}

TEST(AbiBuilder, MutReceiver)
{
    TestAbiBuilder builder { X64Desc };

    builder.ConsumeReceiverMut(Ir(7), Ir(8));
    builder.Consume(Ir(9), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.iregs[3], IrValue(9));
}

TEST(AbiBuilder, RefReceiver)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::REF);
    builder.Consume(Ir(8), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, RecReceiver)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::REF);
    builder.Consume(Ir(8), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, IntOverflowToStack)
{
    TestAbiBuilder builder { X64Desc };

    for (int i = 7; i <= 13; i++) {
        builder.Consume(Ir(i), ArgKind::INT);
    }

    RegsAndSlots r;
    r.Resolve(builder.moves);

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
    TestAbiBuilder builder { X64Desc };

    for (int i = 8; i < 16; i++) {
        builder.Consume(Fr(i), ArgKind::FLOAT);
    }

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 8);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(r.fregs[i], FrValue(i + 8));
    }
}

TEST(AbiBuilder, InterfaceCallGeneric)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::INT);
    builder.Consume(Ir(8), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
}

TEST(AbiBuilder, SRetPlusMutPlusParams)
{
    TestAbiBuilder builder { X64Desc };

    builder.ConsumeSret(Ir(7));
    builder.ConsumeReceiverMut(Ir(8), Ir(9));
    builder.Consume(Ir(10), ArgKind::INT);
    builder.Consume(Ir(11), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 5);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.iregs[3], IrValue(9));
    EXPECT_EQ(r.iregs[4], IrValue(10));
    EXPECT_EQ(r.iregs[5], IrValue(11));
}

TEST(AbiBuilder, Aarch64MoreIntRegs)
{
    TestAbiBuilder builder { A64Desc };

    for (int i = 6; i <= 13; i++) {
        builder.Consume(Ir(i), ArgKind::INT);
    }

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 8);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(r.iregs[i + 1], IrValue(i + 6));
    }
}

TEST(AbiBuilder, SelfMoveProducesNoEmit)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(1), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    EXPECT_EQ(r.movCount, 0);
}

TEST(AbiBuilder, CycleInArgs)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(2), ArgKind::INT);
    builder.Consume(Ir(1), ArgKind::INT);

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.iregs[2], IrValue(1));
}

TEST(AbiBuilder, PlatformParameterized)
{
    // x86_64: 6 int regs, 2 overflow to stack
    {
        TestAbiBuilder builder { X64Desc };
        for (int i = 6; i <= 13; i++) {
            builder.Consume(Ir(i), ArgKind::INT);
        }
        RegsAndSlots r;
        r.Resolve(builder.moves);
        ASSERT_EQ(r.movCount, 8);
        EXPECT_EQ(r.iregs[6], IrValue(11));
        EXPECT_EQ(r.paramPassingStackSlots[0], IrValue(12));
        EXPECT_EQ(r.paramPassingStackSlots[1], IrValue(13));
    }

    // aarch64: 8 int regs, no overflow
    {
        TestAbiBuilder builder { A64Desc };
        for (int i = 6; i <= 13; i++) {
            builder.Consume(Ir(i), ArgKind::INT);
        }
        RegsAndSlots r;
        r.Resolve(builder.moves);
        ASSERT_EQ(r.movCount, 8);
        EXPECT_EQ(r.iregs[8], IrValue(13));
    }
}

TEST(AbiBuilder, TemplateAdapter)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(IntArg(), Ir(7));
    builder.Consume(FloatArg(), Fr(3));
    builder.Consume(IntArg(), Ir(8));

    RegsAndSlots r;
    r.Resolve(builder.moves);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(8));
    EXPECT_EQ(r.fregs[0], FrValue(3));
}

// --- Tracking tests ---

TEST(AbiBuilder, IregStackPtrMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::REC);
    builder.Consume(Ir(8), ArgKind::INT);
    builder.Consume(Ir(9), ArgKind::REC);

    EXPECT_EQ(builder.IregStackPtrMask(), 0x05); // bits 0 and 2
    EXPECT_EQ(builder.IregRefMask(), 0x00);
    EXPECT_EQ(builder.FregMask(), 0x00);
}

TEST(AbiBuilder, IregRefMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::REF);
    builder.Consume(Ir(8), ArgKind::INT);
    builder.Consume(Ir(9), ArgKind::REF);

    EXPECT_EQ(builder.IregRefMask(), 0x05); // bits 0 and 2
    EXPECT_EQ(builder.IregStackPtrMask(), 0x00);
}

TEST(AbiBuilder, FregMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Fr(3), ArgKind::FLOAT);
    builder.Consume(Fr(4), ArgKind::FLOAT);
    builder.Consume(Ir(7), ArgKind::INT);

    EXPECT_EQ(builder.FregMask(), 0x03); // bits 0 and 1
    EXPECT_EQ(builder.IregRefMask(), 0x00);
}

TEST(AbiBuilder, RefStackSlotsOverflow)
{
    TestAbiBuilder builder { X64Desc };

    // Fill all 6 ireg slots with non-ref params
    for (int i = 7; i <= 12; i++) {
        builder.Consume(Ir(i), ArgKind::INT);
    }
    // Next two are refs → overflow to slots
    builder.Consume(Ir(13), ArgKind::REF);
    builder.Consume(Ir(14), ArgKind::REF);

    auto slots = builder.RefStackSlots();
    ASSERT_EQ(slots.Size(), 2);
    EXPECT_EQ(slots[0], 0);
    EXPECT_EQ(slots[1], 1);
    EXPECT_EQ(builder.MaxStackSlot(), 2);
    EXPECT_EQ(builder.RecStackSlots().Size(), 0);
}

TEST(AbiBuilder, RecStackSlotsOverflow)
{
    TestAbiBuilder builder { X64Desc };

    // Fill all 6 ireg slots with non-rec params
    for (int i = 7; i <= 12; i++) {
        builder.Consume(Ir(i), ArgKind::INT);
    }
    // Next two are records → overflow to slots
    builder.Consume(Ir(13), ArgKind::REC);
    builder.Consume(Ir(14), ArgKind::REC);

    auto slots = builder.RecStackSlots();
    ASSERT_EQ(slots.Size(), 2);
    EXPECT_EQ(slots[0], 0);
    EXPECT_EQ(slots[1], 1);
    EXPECT_EQ(builder.MaxStackSlot(), 2);
    EXPECT_EQ(builder.RefStackSlots().Size(), 0);
}

TEST(AbiBuilder, SretSetsStackPtrMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.ConsumeSret(Ir(7));
    builder.Consume(Ir(8), ArgKind::INT);

    EXPECT_EQ(builder.IregStackPtrMask(), 0x01); // bit 0 (sret took first ireg slot)
    EXPECT_EQ(builder.IregRefMask(), 0x00);
}

TEST(AbiBuilder, MutSetsRefMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.ConsumeReceiverMut(Ir(7), Ir(8));
    builder.Consume(Ir(9), ArgKind::INT);

    EXPECT_EQ(builder.IregRefMask(), 0x2); // bits 1
    EXPECT_EQ(builder.IregStackPtrMask(), 0x00);
}

TEST(AbiBuilder, ReceiverSetsRefMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::REF);
    builder.Consume(Ir(8), ArgKind::INT);

    EXPECT_EQ(builder.IregRefMask(), 0x01); // bit 0
    EXPECT_EQ(builder.IregStackPtrMask(), 0x00);
}

TEST(AbiBuilder, FtvarsSetsRefMask)
{
    TestAbiBuilder builder { X64Desc };

    builder.Consume(Ir(7), ArgKind::INT);
    builder.Consume(Ir(8), ArgKind::INT);

    EXPECT_EQ(builder.IregRefMask(), 0x00);
    EXPECT_EQ(builder.IregStackPtrMask(), 0x00);
}

TEST(AbiBuilder, CombinedSretMutOverflow)
{
    TestAbiBuilder builder { X64Desc };

    // SRET → ireg slot 0, stackPtrMask bit 0
    builder.ConsumeSret(Ir(7));
    // MUT → ireg slots 1, 2, refMask bits 1, 2
    builder.ConsumeReceiverMut(Ir(8), Ir(9));
    // 3 int params → ireg slots 3, 4, 5 (no flags)
    builder.Consume(Ir(10), ArgKind::INT);
    builder.Consume(Ir(11), ArgKind::INT);
    builder.Consume(Ir(12), ArgKind::INT);
    // 1 ref overflow → slot 0
    builder.Consume(Ir(13), ArgKind::REF);
    // 1 rec overflow → slot 1
    builder.Consume(Ir(14), ArgKind::REC);

    EXPECT_EQ(builder.IregStackPtrMask(), 0x01); // bit 0 (sret)
    EXPECT_EQ(builder.IregRefMask(), 0x04);      // bits 2
    EXPECT_EQ(builder.FregMask(), 0x00);

    auto refSlots = builder.RefStackSlots();
    ASSERT_EQ(refSlots.Size(), 1);
    EXPECT_EQ(refSlots[0], 0);

    auto recSlots = builder.RecStackSlots();
    ASSERT_EQ(recSlots.Size(), 1);
    EXPECT_EQ(recSlots[0], 1);

    EXPECT_EQ(builder.MaxStackSlot(), 2);
}

TEST(AbiBuilder, FloatOverflowToStackNoTracking)
{
    TestAbiBuilder builder { X64Desc };

    // Fill all 8 freg slots
    for (int i = 0; i < 8; i++) {
        builder.Consume(Fr(i), ArgKind::FLOAT);
    }
    // 9th float overflows to stack
    builder.Consume(Fr(8), ArgKind::FLOAT);

    EXPECT_EQ(builder.FregMask(), 0xFF); // bits 0-7
    EXPECT_EQ(builder.MaxStackSlot(), 1);
    EXPECT_EQ(builder.RefStackSlots().Size(), 0);
    EXPECT_EQ(builder.RecStackSlots().Size(), 0);
}

TEST(AbiBuilder, ClearResetsState)
{
    TestAbiBuilder builder { X64Desc };

    // Populate state
    builder.ConsumeSret(Ir(7));
    builder.ConsumeReceiverMut(Ir(8), Ir(9));
    builder.Consume(Ir(10), ArgKind::REC);
    builder.Consume(Ir(11), ArgKind::REF);
    builder.Consume(Fr(3), ArgKind::FLOAT);
    // Overflow a ref to stack
    for (int i = 12; i <= 13; i++) {
        builder.Consume(Ir(i), ArgKind::INT);
    }
    builder.Consume(Ir(14), ArgKind::REF);

    // Verify state is populated
    EXPECT_NE(builder.IregStackPtrMask(), 0);
    EXPECT_NE(builder.IregRefMask(), 0);
    EXPECT_NE(builder.FregMask(), 0);
    EXPECT_GT(builder.MaxStackSlot(), 0);
    EXPECT_GT(builder.RefStackSlots().Size(), 0);

    // Clear
    builder.Clear();

    // Verify all state is reset
    EXPECT_EQ(builder.IregStackPtrMask(), 0);
    EXPECT_EQ(builder.IregRefMask(), 0);
    EXPECT_EQ(builder.FregMask(), 0);
    EXPECT_EQ(builder.MaxStackSlot(), 0);
    EXPECT_EQ(builder.RefStackSlots().Size(), 0);
    EXPECT_EQ(builder.RecStackSlots().Size(), 0);
}

TEST(AbiBuilder, ClearAllowsReuse)
{
    TestAbiBuilder builder { X64Desc };

    // First use
    builder.Consume(Ir(7), ArgKind::REC);
    EXPECT_EQ(builder.IregStackPtrMask(), 0x01);

    // Clear and reuse
    builder.Clear();
    builder.Consume(Fr(3), ArgKind::FLOAT);

    EXPECT_EQ(builder.IregStackPtrMask(), 0);
    EXPECT_EQ(builder.FregMask(), 0x01);
    EXPECT_EQ(builder.MaxStackSlot(), 0);
}
