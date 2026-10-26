#include <cstring>
#include <gtest/gtest.h>

#include "cbc/isa.h"
#include "cbc/move_resolver.h"
#include "regs_and_slots.h"

using namespace Cbc;

namespace {

using LDK = Format::LoadAccessKind;

struct Emit {
    Location dst;
    Location src; // move source or load address
    int32_t offset;
    LDK ldk;
    bool isLoad;

    Emit(Location dst, Location src) : dst(dst), src(src), offset(0), ldk(LDK::LD_64), isLoad(false) {}

    Emit(Location dst, Location addr, int32_t offset, LDK ldk)
        : dst(dst),
          src(addr),
          offset(offset),
          ldk(ldk),
          isLoad(true)
    {}

    bool operator==(const Emit& other) const
    {
        return dst == other.dst && src == other.src && offset == other.offset && ldk == other.ldk &&
               isLoad == other.isLoad;
    }
};

struct RecordingEmitter : public MoveEmitter {
    std::vector<Emit> emits;

    void AssignIReg(IReg dst, IReg src) override { emits.emplace_back(Location::IReg(dst), Location::IReg(src)); }

    void AssignFReg(FReg dst, FReg src) override { emits.emplace_back(Location::FReg(dst), Location::FReg(src)); }

    void AssignParamSlot(int paramSlot, int stackSlot) override
    {
        emits.emplace_back(Location::Slot(paramSlot), Location::Slot(stackSlot));
    }

    void AssignFRegFromSlot(FReg dst, int slot) override
    {
        emits.emplace_back(Location::FReg(dst), Location::Slot(slot));
    }

    void AssignIRegFromSlot(IReg dst, int slot) override
    {
        emits.emplace_back(Location::IReg(dst), Location::Slot(slot));
    }

    void AssignSlotFromIReg(int slot, IReg src) override
    {
        emits.emplace_back(Location::Slot(slot), Location::IReg(src));
    }

    void AssignSlotFromFReg(int slot, FReg src) override
    {
        emits.emplace_back(Location::Slot(slot), Location::FReg(src));
    }

    void AssignIRegFromAddr(IReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        emits.emplace_back(Location::IReg(dst), addr, offset, ldk);
    }

    void AssignFRegFromAddr(FReg dst, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        emits.emplace_back(Location::FReg(dst), addr, offset, ldk);
    }

    void AssignSlotFromAddr(int paramSlot, Location addr, int32_t offset, Format::LoadAccessKind ldk) override
    {
        emits.emplace_back(Location::Slot(paramSlot), addr, offset, ldk);
    }
};

std::vector<Emit> Resolve(MoveResolver& mr)
{
    RecordingEmitter emitter;
    mr.Resolve(emitter);
    return emitter.emits;
}

} // namespace

TEST(MoveResolver, NoConflict)
{
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[4], IrValue(3));
}

TEST(MoveResolver, TwoCycleSwap)
{
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 1 }); // IR2 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
}

TEST(MoveResolver, ThreeCycle)
{
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 3 }); // IR2 -> IR3
    mr.AddMove(Location { 3 }, Location { 1 }); // IR3 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(2));
    EXPECT_EQ(r.iregs[1], IrValue(3));
}

TEST(MoveResolver, FRegCycle)
{
    MoveResolver mr;
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 15 }, Location { 14 }); // FR1 -> FR0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[0], FrValue(1));
}

TEST(MoveResolver, SlotCycle)
{
    MoveResolver mr;
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 31 }, Location { 30 }); // Slot1 -> Slot0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[0], StValue(1));
}

TEST(MoveResolver, SingleMove)
{
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 });

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 1);
    EXPECT_EQ(r.iregs[2], IrValue(1));
}

TEST(MoveResolver, Empty)
{
    MoveResolver mr;
    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 0);
}

TEST(MoveResolver, ClearAllowsReuse)
{
    MoveResolver mr;
    RegsAndSlots r1;
    r1.Resolve(mr);

    // First round
    mr.AddMove(Location { 1 }, Location { 2 });
    r1.Resolve(mr);
    ASSERT_EQ(r1.movCount, 1);
    EXPECT_EQ(r1.iregs[2], IrValue(1));

    // Clear and reuse
    mr.Clear();
    RegsAndSlots r2;
    mr.AddMove(Location { 5 }, Location { 6 });
    mr.AddMove(Location { 6 }, Location { 5 });
    r2.Resolve(mr);
    ASSERT_EQ(r2.movCount, 3); // 2 moves + 1 cycle break
    EXPECT_EQ(r2.iregs[5], IrValue(6));
    EXPECT_EQ(r2.iregs[6], IrValue(5));
}

TEST(MoveResolver, LocationKind)
{
    EXPECT_EQ(Location { 0 }.Kind(), Location::IREG);
    EXPECT_EQ(Location { 13 }.Kind(), Location::IREG);
    EXPECT_EQ(Location { 14 }.Kind(), Location::FREG);
    EXPECT_EQ(Location { 29 }.Kind(), Location::FREG);
    EXPECT_EQ(Location { 30 }.Kind(), Location::SLOT);
    EXPECT_EQ(Location { 35 }.Kind(), Location::SLOT);

    EXPECT_EQ(Location { 14 }.FRegIdx(), 0u);
    EXPECT_EQ(Location { 29 }.FRegIdx(), 15u);
    EXPECT_EQ(Location { 30 }.SlotIdx(), 0u);
    EXPECT_EQ(Location { 35 }.SlotIdx(), 5u);
}

// --- Similar resource tests: dst and src from the same resource class ---

TEST(MoveResolver, SimilarIRegNoCycle)
{
    // Both src and dst are param-passing IRs (IR1-IR6 on x64), no cycles.
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4
    mr.AddMove(Location { 5 }, Location { 6 }); // IR5 -> IR6

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[6], IrValue(5));
}

TEST(MoveResolver, SimilarIRegCycle)
{
    // 4-cycle among param-passing IRs: IR1->IR2->IR3->IR4->IR1
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 3 }); // IR2 -> IR3
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4
    mr.AddMove(Location { 4 }, Location { 1 }); // IR4 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 5); // 4 moves + 1 temp break
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(2));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[1], IrValue(4));
}

TEST(MoveResolver, SimilarFRegNoCycle)
{
    // Both src and dst are param-passing FRs (FR0-FR7), no cycles.
    MoveResolver mr;
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 16 }, Location { 17 }); // FR2 -> FR3
    mr.AddMove(Location { 18 }, Location { 19 }); // FR4 -> FR5

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[3], FrValue(2));
    EXPECT_EQ(r.fregs[5], FrValue(4));
}

TEST(MoveResolver, SimilarFRegCycle)
{
    // 3-cycle among param-passing FRs: FR0->FR1->FR2->FR0
    MoveResolver mr;
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 15 }, Location { 16 }); // FR1 -> FR2
    mr.AddMove(Location { 16 }, Location { 14 }); // FR2 -> FR0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 4); // 3 moves + 1 temp break
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[2], FrValue(1));
    EXPECT_EQ(r.fregs[0], FrValue(2));
}

TEST(MoveResolver, SimilarSlotNoCycle)
{
    // Both src and dst are stack slots, no cycles (impossible per doc).
    MoveResolver mr;
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3
    mr.AddMove(Location { 34 }, Location { 35 }); // Slot4 -> Slot5

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
    EXPECT_EQ(r.paramPassingStackSlots[5], StValue(4));
}

// --- Doc invariant: volatile src -> param-passing dst (no cycle) ---

TEST(MoveResolver, VolatileSrcToParamDst)
{
    // IR7 is TEMP_IR (volatile, walked first), IR1 is param-passing.
    // Doc: "all of them can be used as src", "only param-passing ones can be used as dst"
    MoveResolver mr;
    mr.AddMove(Location { 7 }, Location { 1 }); // IR7 (volatile/temp) -> IR1 (param)

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 1);
    EXPECT_EQ(r.iregs[1], IrValue(7));
}

TEST(MoveResolver, VolatileSrcChainToParamDst)
{
    // Chain: IR7 (volatile/temp) -> IR1 -> IR2 (param-passing)
    // IR7 is walked first as TEMP_IR, so the chain resolves without false cycle.
    MoveResolver mr;
    mr.AddMove(Location { 7 }, Location { 1 }); // IR7 -> IR1
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[1], IrValue(7));
    EXPECT_EQ(r.iregs[2], IrValue(1));
}

// --- Full param-passing cycle (stress) ---

TEST(MoveResolver, FullIRegParamCycle)
{
    // 6-cycle: all x64 param-passing IRs (IR1-IR6)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 3 }); // IR2 -> IR3
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4
    mr.AddMove(Location { 4 }, Location { 5 }); // IR4 -> IR5
    mr.AddMove(Location { 5 }, Location { 6 }); // IR5 -> IR6
    mr.AddMove(Location { 6 }, Location { 1 }); // IR6 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 7); // 6 moves + 1 temp break
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(2));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[5], IrValue(4));
    EXPECT_EQ(r.iregs[6], IrValue(5));
    EXPECT_EQ(r.iregs[1], IrValue(6));
}

TEST(MoveResolver, FullFRegParamCycle)
{
    // 8-cycle: all x64 param-passing FRs (FR0-FR7)
    MoveResolver mr;
    for (int i = 0; i < 7; i++) {
        mr.AddMove(Location { 14 + i }, Location { 15 + i }); // FRi -> FR(i+1)
    }
    mr.AddMove(Location { 21 }, Location { 14 }); // FR7 -> FR0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 9); // 8 moves + 1 temp break
    for (int i = 0; i < 7; i++) {
        EXPECT_EQ(r.fregs[i + 1], FrValue(i));
    }
    EXPECT_EQ(r.fregs[0], FrValue(7));
}

// --- Multiple independent cycles ---

TEST(MoveResolver, MultipleIndependentCycles)
{
    // Two independent 2-cycles: IR1<->IR2 and IR3<->IR4
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 1 }); // IR2 -> IR1
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4
    mr.AddMove(Location { 4 }, Location { 3 }); // IR4 -> IR3

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 6); // 2 cycles x (2 moves + 1 temp break)
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[3], IrValue(4));
}

TEST(MoveResolver, ThreeIndependentCyclesMixed)
{
    // IR 2-cycle + FR 2-cycle + Slot pair (no slot cycle)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 });   // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 1 });   // IR2 -> IR1
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 15 }, Location { 14 }); // FR1 -> FR0
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3

    RegsAndSlots r;
    r.Resolve(mr);

    // IR cycle: 3, FR cycle: 3, Slots: 2
    ASSERT_EQ(r.movCount, 8);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[0], FrValue(1));
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
}

TEST(MoveResolver, IndependentCyclesWithSlotMoves)
{
    // IR 3-cycle + FR 2-cycle + 3 independent slot moves
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 });   // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 3 });   // IR2 -> IR3
    mr.AddMove(Location { 3 }, Location { 1 });   // IR3 -> IR1
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 15 }, Location { 14 }); // FR1 -> FR0
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3
    mr.AddMove(Location { 34 }, Location { 35 }); // Slot4 -> Slot5

    RegsAndSlots r;
    r.Resolve(mr);

    // IR 3-cycle: 4, FR 2-cycle: 3, Slots: 3
    ASSERT_EQ(r.movCount, 10);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(2));
    EXPECT_EQ(r.iregs[1], IrValue(3));
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[0], FrValue(1));
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
    EXPECT_EQ(r.paramPassingStackSlots[5], StValue(4));
}

TEST(MoveResolver, ManySlotMovesNoCycle)
{
    // 6 independent slot moves, no register moves
    MoveResolver mr;
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3
    mr.AddMove(Location { 34 }, Location { 35 }); // Slot4 -> Slot5
    mr.AddMove(Location { 36 }, Location { 37 }); // Slot6 -> Slot7
    mr.AddMove(Location { 38 }, Location { 39 }); // Slot8 -> Slot9
    mr.AddMove(Location { 40 }, Location { 41 }); // Slot10 -> Slot11

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 6);
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
    EXPECT_EQ(r.paramPassingStackSlots[5], StValue(4));
    EXPECT_EQ(r.paramPassingStackSlots[7], StValue(6));
    EXPECT_EQ(r.paramPassingStackSlots[9], StValue(8));
    EXPECT_EQ(r.paramPassingStackSlots[11], StValue(10));
}

TEST(MoveResolver, SlotMovesWithSingleRegCycle)
{
    // One IR 2-cycle + 4 slot moves
    MoveResolver mr;
    mr.AddMove(Location { 5 }, Location { 6 });   // IR5 -> IR6
    mr.AddMove(Location { 6 }, Location { 5 });   // IR6 -> IR5
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3
    mr.AddMove(Location { 34 }, Location { 35 }); // Slot4 -> Slot5
    mr.AddMove(Location { 36 }, Location { 37 }); // Slot6 -> Slot7

    RegsAndSlots r;
    r.Resolve(mr);

    // IR cycle: 3, Slots: 4
    ASSERT_EQ(r.movCount, 7);
    EXPECT_EQ(r.iregs[6], IrValue(5));
    EXPECT_EQ(r.iregs[5], IrValue(6));
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
    EXPECT_EQ(r.paramPassingStackSlots[5], StValue(4));
    EXPECT_EQ(r.paramPassingStackSlots[7], StValue(6));
}

TEST(MoveResolver, ThreeIndependentIRCycles)
{
    // Three independent 2-cycles among IRs: (1,2), (3,4), (5,6)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 1 }); // IR2 -> IR1
    mr.AddMove(Location { 3 }, Location { 4 }); // IR3 -> IR4
    mr.AddMove(Location { 4 }, Location { 3 }); // IR4 -> IR3
    mr.AddMove(Location { 5 }, Location { 6 }); // IR5 -> IR6
    mr.AddMove(Location { 6 }, Location { 5 }); // IR6 -> IR5

    RegsAndSlots r;
    r.Resolve(mr);

    // 3 cycles x (2 + 1 temp) = 9
    ASSERT_EQ(r.movCount, 9);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[3], IrValue(4));
    EXPECT_EQ(r.iregs[6], IrValue(5));
    EXPECT_EQ(r.iregs[5], IrValue(6));
}

TEST(MoveResolver, MixedCycleSizesWithSlots)
{
    // IR 2-cycle + IR 3-cycle + FR 2-cycle + 2 slot moves
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 });   // IR1 -> IR2
    mr.AddMove(Location { 2 }, Location { 1 });   // IR2 -> IR1
    mr.AddMove(Location { 3 }, Location { 4 });   // IR3 -> IR4
    mr.AddMove(Location { 4 }, Location { 5 });   // IR4 -> IR5
    mr.AddMove(Location { 5 }, Location { 3 });   // IR5 -> IR3
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 15 }, Location { 14 }); // FR1 -> FR0
    mr.AddMove(Location { 30 }, Location { 31 }); // Slot0 -> Slot1
    mr.AddMove(Location { 32 }, Location { 33 }); // Slot2 -> Slot3

    RegsAndSlots r;
    r.Resolve(mr);

    // IR 2-cycle: 3, IR 3-cycle: 4, FR 2-cycle: 3, Slots: 2
    ASSERT_EQ(r.movCount, 12);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.iregs[4], IrValue(3));
    EXPECT_EQ(r.iregs[5], IrValue(4));
    EXPECT_EQ(r.iregs[3], IrValue(5));
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[0], FrValue(1));
    EXPECT_EQ(r.paramPassingStackSlots[1], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[3], StValue(2));
}

// --- Fan-out: one src used by multiple dsts ---

TEST(MoveResolver, FanOutNoCycle)
{
    // IR1 -> IR2, IR1 -> IR3 (IR1 broadcast to two dsts, no cycle)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(1));
}

TEST(MoveResolver, FanOutWithCycle)
{
    // IR1, IR2, IR3 = IR3, IR3, IR1
    // i.e. IR3->IR1, IR3->IR2, IR1->IR3
    // IR3 is broadcast to IR1 and IR2; IR1 feeds back to IR3 (cycle).
    MoveResolver mr;
    mr.AddMove(Location { 3 }, Location { 1 }); // IR3 -> IR1
    mr.AddMove(Location { 3 }, Location { 2 }); // IR3 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3

    RegsAndSlots r;
    r.Resolve(mr);

    // Cycle break (1 temp) + 3 assignments = 4
    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[1], IrValue(3));
    EXPECT_EQ(r.iregs[2], IrValue(3));
    EXPECT_EQ(r.iregs[3], IrValue(1));
}

TEST(MoveResolver, FanOutTriple)
{
    // IR1 -> IR2, IR1 -> IR3, IR1 -> IR4 (triple broadcast, no cycle)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3
    mr.AddMove(Location { 1 }, Location { 4 }); // IR1 -> IR4

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(1));
    EXPECT_EQ(r.iregs[4], IrValue(1));
}

TEST(MoveResolver, FanOutToSlots)
{
    // IR1 -> Slot0, IR1 -> Slot1 (register broadcast to two slots)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 30 }); // IR1 -> Slot0
    mr.AddMove(Location { 1 }, Location { 31 }); // IR1 -> Slot1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.paramPassingStackSlots[0], IrValue(1));
    EXPECT_EQ(r.paramPassingStackSlots[1], IrValue(1));
}

TEST(MoveResolver, FanOutMultipleSrcs)
{
    // IR1 -> IR2, IR1 -> IR3, IR4 -> IR5, IR4 -> IR6
    // Two independent broadcasts, no cycles.
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3
    mr.AddMove(Location { 4 }, Location { 5 }); // IR4 -> IR5
    mr.AddMove(Location { 4 }, Location { 6 }); // IR4 -> IR6

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(1));
    EXPECT_EQ(r.iregs[5], IrValue(4));
    EXPECT_EQ(r.iregs[6], IrValue(4));
}

TEST(MoveResolver, FanOutWithIndependentCycle)
{
    // IR1 -> IR2, IR1 -> IR3 (broadcast) + IR4 <-> IR5 (independent cycle)
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3
    mr.AddMove(Location { 4 }, Location { 5 }); // IR4 -> IR5
    mr.AddMove(Location { 5 }, Location { 4 }); // IR5 -> IR4

    RegsAndSlots r;
    r.Resolve(mr);

    // Fan-out: 2, Cycle: 3 = 5
    ASSERT_EQ(r.movCount, 5);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(1));
    EXPECT_EQ(r.iregs[5], IrValue(4));
    EXPECT_EQ(r.iregs[4], IrValue(5));
}

TEST(MoveResolver, FanOutFRegWithCycle)
{
    // FR0 -> FR1, FR0 -> FR2, FR1 -> FR0
    // FR0 broadcast to FR1 and FR2; FR1 feeds back to FR0.
    MoveResolver mr;
    mr.AddMove(Location { 14 }, Location { 15 }); // FR0 -> FR1
    mr.AddMove(Location { 14 }, Location { 16 }); // FR0 -> FR2
    mr.AddMove(Location { 15 }, Location { 14 }); // FR1 -> FR0

    RegsAndSlots r;
    r.Resolve(mr);

    // Cycle break (1 temp) + 3 assignments = 4
    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.fregs[1], FrValue(0));
    EXPECT_EQ(r.fregs[2], FrValue(0));
    EXPECT_EQ(r.fregs[0], FrValue(1));
}

TEST(MoveResolver, FanOutChain)
{
    // IR1 -> IR2, IR1 -> IR3, IR2 -> IR4
    // Simultaneous semantics: all RHS are original values.
    // IR4 := IR2_orig (not IR2's updated value).
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 }); // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 3 }); // IR1 -> IR3
    mr.AddMove(Location { 2 }, Location { 4 }); // IR2 -> IR4

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[3], IrValue(1));
    EXPECT_EQ(r.iregs[4], IrValue(2)); // IR2's original value
}

TEST(MoveResolver, FanOutWithSlotAndCycle)
{
    // IR1 -> IR2, IR1 -> Slot0, IR2 -> IR1
    // IR1 broadcasts to IR2 and Slot0; IR2 feeds back to IR1.
    MoveResolver mr;
    mr.AddMove(Location { 1 }, Location { 2 });  // IR1 -> IR2
    mr.AddMove(Location { 1 }, Location { 30 }); // IR1 -> Slot0
    mr.AddMove(Location { 2 }, Location { 1 });  // IR2 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    // Cycle break (1 temp) + IR2:=IR1 + Slot0:=IR1 + IR1:=temp = 4
    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[2], IrValue(1));
    EXPECT_EQ(r.iregs[1], IrValue(2));
    EXPECT_EQ(r.paramPassingStackSlots[0], IrValue(1));
}

TEST(MoveResolver, RegressionMatrixMultiply_parallel_bench)
{
    MoveResolver mr;
    using namespace Cbc;

    // clang-format off
    mr.AddMove(Location::IReg(IReg::IR1),  Location::IReg(IReg::IR1));
    mr.AddMove(Location::IReg(IReg::IR2),  Location::IReg(IReg::IR2));
    mr.AddMove(Location::IReg(IReg::IR9),  Location::IReg(IReg::IR3));
    mr.AddMove(Location::IReg(IReg::IR10), Location::IReg(IReg::IR4));
    mr.AddMove(Location::IReg(IReg::IR11), Location::IReg(IReg::IR5));
    mr.AddMove(Location::IReg(IReg::IR13), Location::IReg(IReg::IR6));
    mr.AddMove(Location::Slot(0),          Location::Slot(0));
    mr.AddMove(Location::IReg(IReg::IR12), Location::Slot(1));
    mr.AddMove(Location::IReg(IReg::IR8),  Location::Slot(2));
    // clang-format on

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 7);
    EXPECT_EQ(r.iregs[1], IrValue(1));
    EXPECT_EQ(r.iregs[2], IrValue(2));
    EXPECT_EQ(r.iregs[3], IrValue(9));
    EXPECT_EQ(r.iregs[4], IrValue(10));
    EXPECT_EQ(r.iregs[5], IrValue(11));
    EXPECT_EQ(r.iregs[6], IrValue(13));
    EXPECT_EQ(r.paramPassingStackSlots[0], StValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[1], IrValue(12));
    EXPECT_EQ(r.paramPassingStackSlots[2], IrValue(8));
}

// --- Load-from-address tests ---

TEST(MoveResolver, LoadIntoIRegFromIRegAddr)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_32, Location::IReg(IReg::IR2)); // IR2 = *(IR1 + 8)

    std::vector<Emit> expected { Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 8, LDK::LD_32) };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadIntoFRegFromIRegAddr)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_F64, Location::FReg(FReg::FR0)); // FR0 = *(IR1)

    std::vector<Emit> expected { Emit(Location::FReg(FReg::FR0), Location::IReg(IReg::IR1), 0, LDK::LD_F64) };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadIntoSlotFromIRegAddr)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 16, LDK::LD_64, Location::Slot(0)); // Slot0 = *(IR1 + 16)

    std::vector<Emit> expected { Emit(Location::Slot(0), Location::IReg(IReg::IR1), 16, LDK::LD_64) };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadFromSlotAddrIntoRegs)
{
    // Untyped slots are immutable: loads from them are emitted after the register shuffle.
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::Slot(2), 8, LDK::LD_U16, Location::IReg(IReg::IR3)); // IR3 = *(Slot2 + 8)
    mr.AddLoadFromAddr(Location::Slot(3), 0, LDK::LD_F32, Location::FReg(FReg::FR1)); // FR1 = *(Slot3)

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR3), Location::Slot(2), 8, LDK::LD_U16),
        Emit(Location::FReg(FReg::FR1), Location::Slot(3), 0, LDK::LD_F32),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadFromSlotAddrIntoSlot)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::Slot(2), 8, LDK::LD_64, Location::Slot(0)); // Slot0 = *(Slot2 + 8)

    std::vector<Emit> expected { Emit(Location::Slot(0), Location::Slot(2), 8, LDK::LD_64) };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadToSlotIsAtTheBeginning)
{
    // Param slot loads are emitted before the register shuffle and read the address first.
    MoveResolver mr;
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                // IR1 = IR5
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_64, Location::Slot(0)); // Slot0 = *(IR1 + 8)

    std::vector<Emit> expected {
        Emit(Location::Slot(0), Location::IReg(IReg::IR1), 8, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, SlotAddrLoadIsDeferred)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::Slot(2), 8, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(Slot2 + 8)
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                // IR1 = IR5

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
        Emit(Location::IReg(IReg::IR2), Location::Slot(2), 8, LDK::LD_64),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, ILoadBeforeMove)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                        // IR1 = IR5

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, FLoadBeforeMove)
{
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_F64, Location::FReg(FReg::FR0)); // FR0 = *(IR1)
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                         // IR1 = IR5

    std::vector<Emit> expected {
        Emit(Location::FReg(FReg::FR0), Location::IReg(IReg::IR1), 0, LDK::LD_F64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadMoveCycleBrokenWithTemp)
{
    // IR2 = *(IR1); IR1 = IR2 forms a cycle: IR2 is staged through the temp register.
    MoveResolver mr(IReg::IR7);
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddMove(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1));                        // IR1 = IR2

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR7)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadLoadCycleBrokenWithTemp)
{
    // Two loads reading each other's destination as the address:
    //
    // IR1 --load--> IR2
    //  ^             |
    //  |____load_____|
    //
    MoveResolver mr(IReg::IR7);
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR2), 8, LDK::LD_64, Location::IReg(IReg::IR1)); // IR1 = *(IR2 + 8)

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR7), 8, LDK::LD_64),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, SharedAddrLoadsClobberAndFanOut)
{
    // IR3 ---> IR1 --load--> IR2 ---> IR4
    //  ^        |
    //  |__load__|
    //
    MoveResolver mr(IReg::IR7);
    mr.AddMove(Location::IReg(IReg::IR3), Location::IReg(IReg::IR1));                        // IR1 = IR3
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_64, Location::IReg(IReg::IR3)); // IR3 = *(IR1 + 8)
    mr.AddMove(Location::IReg(IReg::IR2), Location::IReg(IReg::IR4));                        // IR4 = IR2

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR4), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR3)),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR1), 8, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR7)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadSelfAddrPreservesAddr)
{
    // Load spoils IR1, need to be delayed after move.
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 16, LDK::LD_64, Location::IReg(IReg::IR1)); // IR1 = *(IR1 + 16)
    mr.AddMove(Location::IReg(IReg::IR1), Location::IReg(IReg::IR2));                         // IR2 = IR1

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1)),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR1), 16, LDK::LD_64),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, SharedAddrLoadsBeforeMove)
{
    // Two loads from addr on IR1 before IR1 spoiling.
    MoveResolver mr;
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_64, Location::IReg(IReg::IR3)); // IR3 = *(IR1 + 8)
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                        // IR1 = IR5

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR1), 8, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, SharedAddrLoadsAfterMove)
{
    // Two loads from addr on IR1 after IR1 spoiling.
    MoveResolver mr;
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                        // IR1 = IR5
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_64, Location::IReg(IReg::IR3)); // IR3 = *(IR1 + 8)

    std::vector<Emit> expected {
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR1), 8, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

// --- Complex mixed move/load tests ---

TEST(MoveResolver, AllSrcDstKindsWithLoads)
{
    // One assignment per (src kind, dst kind, move/load) combination. No cycles.
    //
    // IR5 ---> IR1   IR2 --load--> IR3   (ireg graph)
    // FR2 ---> FR0   IR2 --load--> FR1   (freg graph)
    //
    MoveResolver mr;
    mr.AddMove(Location::Slot(7), Location::FReg(FReg::FR2));                                  // FR2 = Slot7
    mr.AddLoadFromAddr(Location::Slot(8), 8, LDK::LD_S8, Location::IReg(IReg::IR4));           // IR4 = *(Slot8 + 8)
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR1));                          // IR1 = IR5
    mr.AddMove(Location::FReg(FReg::FR2), Location::FReg(FReg::FR0));                          // FR0 = FR2
    mr.AddLoadFromAddr(Location::IReg(IReg::IR2), 8, LDK::LD_64, Location::IReg(IReg::IR3));   // IR3 = *(IR2 + 8)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR2), 16, LDK::LD_F64, Location::FReg(FReg::FR1)); // FR1 = *(IR2 + 16)
    mr.AddMove(Location::IReg(IReg::IR4), Location::Slot(0));                                  // Slot0 = IR4
    mr.AddMove(Location::FReg(FReg::FR3), Location::Slot(1));                                  // Slot1 = FR3
    mr.AddMove(Location::Slot(0), Location::Slot(2));                                          // Slot2 = Slot0
    mr.AddLoadFromAddr(Location::IReg(IReg::IR2), 24, LDK::LD_32, Location::Slot(3));          // Slot3 = *(IR2 + 24)
    mr.AddLoadFromAddr(Location::Slot(4), 0, LDK::LD_U16, Location::Slot(5));                  // Slot5 = *(Slot4)
    mr.AddMove(Location::Slot(6), Location::IReg(IReg::IR6));                                  // IR6 = Slot6
    mr.AddLoadFromAddr(Location::Slot(9), 16, LDK::LD_F32, Location::FReg(FReg::FR3));         // FR3 = *(Slot9 + 16)

    std::vector<Emit> expected {
        Emit(Location::Slot(0), Location::IReg(IReg::IR4)),
        Emit(Location::Slot(1), Location::FReg(FReg::FR3)),
        Emit(Location::Slot(2), Location::Slot(0)),
        Emit(Location::Slot(3), Location::IReg(IReg::IR2), 24, LDK::LD_32),
        Emit(Location::Slot(5), Location::Slot(4), 0, LDK::LD_U16),

        Emit(Location::FReg(FReg::FR0), Location::FReg(FReg::FR2)),
        Emit(Location::FReg(FReg::FR1), Location::IReg(IReg::IR2), 16, LDK::LD_F64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR5)),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR2), 8, LDK::LD_64),

        Emit(Location::FReg(FReg::FR2), Location::Slot(7)),
        Emit(Location::IReg(IReg::IR4), Location::Slot(8), 8, LDK::LD_S8),
        Emit(Location::IReg(IReg::IR6), Location::Slot(6)),
        Emit(Location::FReg(FReg::FR3), Location::Slot(9), 16, LDK::LD_F32),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, MixedCyclesLoadsAndSlots)
{
    // Two cycles:
    //
    // IR1 --load--> IR2 ---> IR3
    //  ^                      |
    //  |______________________|
    //
    // FR1 <---> FR0
    //
    MoveResolver mr(IReg::IR7);
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 0, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1)
    mr.AddMove(Location::IReg(IReg::IR2), Location::IReg(IReg::IR3));                        // IR3 = IR2
    mr.AddMove(Location::IReg(IReg::IR3), Location::IReg(IReg::IR1));                        // IR1 = IR3
    mr.AddMove(Location::FReg(FReg::FR0), Location::FReg(FReg::FR1));                        // FR1 = FR0
    mr.AddMove(Location::FReg(FReg::FR1), Location::FReg(FReg::FR0));                        // FR0 = FR1
    mr.AddMove(Location::IReg(IReg::IR4), Location::Slot(0));                                // Slot0 = IR4
    mr.AddLoadFromAddr(Location::IReg(IReg::IR5), 8, LDK::LD_32, Location::Slot(1));         // Slot1 = *(IR5 + 8)
    mr.AddLoadFromAddr(Location::Slot(2), 16, LDK::LD_F32, Location::FReg(FReg::FR2));       // FR2 = *(Slot2 + 16)
    mr.AddMove(Location::Slot(3), Location::IReg(IReg::IR6));                                // IR6 = Slot3
    mr.AddMove(Location::Slot(4), Location::FReg(FReg::FR3));                                // FR3 = Slot4

    std::vector<Emit> expected {
        Emit(Location::Slot(0), Location::IReg(IReg::IR4)),
        Emit(Location::Slot(1), Location::IReg(IReg::IR5), 8, LDK::LD_32),

        // Resolved FReg cycle
        Emit(Location::FReg(FReg::FR15), Location::FReg(FReg::FR1)),
        Emit(Location::FReg(FReg::FR1), Location::FReg(FReg::FR0)),
        Emit(Location::FReg(FReg::FR0), Location::FReg(FReg::FR15)),

        // Resolved IReg cycle
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR3)),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR7)),

        Emit(Location::FReg(FReg::FR2), Location::Slot(2), 16, LDK::LD_F32),
        Emit(Location::IReg(IReg::IR6), Location::Slot(3)),
        Emit(Location::FReg(FReg::FR3), Location::Slot(4)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, LoadsFromSlotsAfterResolving)
{
    // IR1 --load--> IR2 ---> IR3
    //
    MoveResolver mr;
    mr.AddMove(Location::Slot(0), Location::IReg(IReg::IR1));                                // IR1 = Slot0
    mr.AddLoadFromAddr(Location::IReg(IReg::IR1), 8, LDK::LD_64, Location::IReg(IReg::IR2)); // IR2 = *(IR1 + 8)
    mr.AddMove(Location::IReg(IReg::IR2), Location::IReg(IReg::IR3));                        // IR3 = IR2
    mr.AddLoadFromAddr(Location::Slot(1), 0, LDK::LD_32, Location::IReg(IReg::IR4));         // IR4 = *(Slot1)
    mr.AddMove(Location::FReg(FReg::FR0), Location::FReg(FReg::FR1));                        // FR1 = FR0
    mr.AddLoadFromAddr(Location::Slot(2), 4, LDK::LD_F64, Location::FReg(FReg::FR0));        // FR0 = *(Slot2 + 4)

    std::vector<Emit> expected {
        Emit(Location::FReg(FReg::FR1), Location::FReg(FReg::FR0)),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1), 8, LDK::LD_64),

        // All slot moves/loads emitted after resolution
        Emit(Location::IReg(IReg::IR1), Location::Slot(0)),
        Emit(Location::IReg(IReg::IR4), Location::Slot(1), 0, LDK::LD_32),
        Emit(Location::FReg(FReg::FR0), Location::Slot(2), 4, LDK::LD_F64),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, TwoCyclesOneWithLoad)
{
    // Two independent ireg cycles (one contains a load) sharing the temp register.
    //
    // IR1 <--> IR2
    //
    // IR3 --load--> IR4 ---> IR5
    //  ^                      |
    //  |______________________|
    //
    MoveResolver mr(IReg::IR7);
    mr.AddMove(Location::IReg(IReg::IR1), Location::IReg(IReg::IR2));                        // IR2 = IR1
    mr.AddMove(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1));                        // IR1 = IR2
    mr.AddLoadFromAddr(Location::IReg(IReg::IR3), 0, LDK::LD_64, Location::IReg(IReg::IR4)); // IR4 = *(IR3)
    mr.AddMove(Location::IReg(IReg::IR4), Location::IReg(IReg::IR5));                        // IR5 = IR4
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR3));                        // IR3 = IR5
    mr.AddMove(Location::FReg(FReg::FR0), Location::Slot(0));                                // Slot0 = FR0
    mr.AddLoadFromAddr(Location::Slot(1), 8, LDK::LD_64, Location::FReg(FReg::FR1));         // FR1 = *(Slot1 + 8)
    mr.AddMove(Location::Slot(2), Location::IReg(IReg::IR6));                                // IR6 = Slot2

    std::vector<Emit> expected {
        Emit(Location::Slot(0), Location::FReg(FReg::FR0)),

        // First resolved IReg cycle
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR2)),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR1)),
        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR7)),

        // Second resolved IReg cycle
        Emit(Location::IReg(IReg::IR7), Location::IReg(IReg::IR5)),
        Emit(Location::IReg(IReg::IR5), Location::IReg(IReg::IR4)),
        Emit(Location::IReg(IReg::IR4), Location::IReg(IReg::IR3), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR7)),

        Emit(Location::FReg(FReg::FR1), Location::Slot(1), 8, LDK::LD_64),
        Emit(Location::IReg(IReg::IR6), Location::Slot(2)),
    };
    EXPECT_EQ(Resolve(mr), expected);
}

TEST(MoveResolver, ManyLoadsFromOneAddr)
{
    // Simulate foreign pass-by-value staging: struct pieces loaded from a base
    // address into param regs/slots, mixed with plain argument moves and loads.
    //
    // IR5 ---> IR9 --load--> { IR1, IR2, FR0, Slot0 }
    //
    MoveResolver mr;
    mr.AddMove(Location::IReg(IReg::IR5), Location::IReg(IReg::IR9));                         // IR9 = IR5
    mr.AddLoadFromAddr(Location::IReg(IReg::IR9), 0, LDK::LD_64, Location::IReg(IReg::IR1));  // IR1 = *(IR9 + 0)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR9), 8, LDK::LD_F64, Location::FReg(FReg::FR0)); // FR0 = *(IR9 + 8)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR9), 16, LDK::LD_32, Location::IReg(IReg::IR2)); // IR2 = *(IR9 + 16)
    mr.AddLoadFromAddr(Location::IReg(IReg::IR9), 24, LDK::LD_64, Location::Slot(0));         // Slot0 = *(IR9 + 24)
    mr.AddMove(Location::IReg(IReg::IR10), Location::IReg(IReg::IR3));                        // IR3 = IR10
    mr.AddMove(Location::Slot(5), Location::IReg(IReg::IR4));                                 // IR4 = Slot5
    mr.AddMove(Location::FReg(FReg::FR1), Location::FReg(FReg::FR2));                         // FR2 = FR1
    mr.AddLoadFromAddr(Location::Slot(6), 0, LDK::LD_64, Location::IReg(IReg::IR5));          // IR5 = *(Slot6)
    mr.AddMove(Location::IReg(IReg::IR11), Location::Slot(1));                                // Slot1 = IR11

    std::vector<Emit> expected {
        Emit(Location::Slot(0), Location::IReg(IReg::IR9), 24, LDK::LD_64),
        Emit(Location::Slot(1), Location::IReg(IReg::IR11)),

        Emit(Location::FReg(FReg::FR0), Location::IReg(IReg::IR9), 8, LDK::LD_F64),
        Emit(Location::FReg(FReg::FR2), Location::FReg(FReg::FR1)),

        Emit(Location::IReg(IReg::IR1), Location::IReg(IReg::IR9), 0, LDK::LD_64),
        Emit(Location::IReg(IReg::IR2), Location::IReg(IReg::IR9), 16, LDK::LD_32),
        Emit(Location::IReg(IReg::IR3), Location::IReg(IReg::IR10)),

        // After all loads from IR9
        Emit(Location::IReg(IReg::IR9), Location::IReg(IReg::IR5)),

        Emit(Location::IReg(IReg::IR4), Location::Slot(5)),
        Emit(Location::IReg(IReg::IR5), Location::Slot(6), 0, LDK::LD_64),
    };
    EXPECT_EQ(Resolve(mr), expected);
}
