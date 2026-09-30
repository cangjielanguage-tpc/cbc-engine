#include <cstring>
#include <gtest/gtest.h>

#include "cbc/isa.h"
#include "cbc/move_resolver.h"

using namespace Cbc;

namespace {

using Emit = std::pair<Location, Location>;

static constexpr int addend = 128;

static int irValue(int ireg) { return ireg; }

static int frValue(int freg) { return freg + addend; }

static int stValue(int slot) { return slot + 2 * addend; }

struct RegsAndSlots {
    static constexpr int stackSlotCount = addend;

    int iregs[IReg::VIRT_COUNT];
    int fregs[FReg::COUNT];
    int untyped[stackSlotCount];
    int paramPassingStackSlots[stackSlotCount];

    int movCount = 0;
    bool log;

    RegsAndSlots(bool log = false) : log(log) {
        for (int i = 0; i < IReg::VIRT_COUNT; i++) iregs[i] = irValue(i);
        for (int i = 0; i < FReg::COUNT; i++) fregs[i] = frValue(i);
        for (int i = 0; i < stackSlotCount; i++) untyped[i] = stValue(i);
        for (int i = 0; i < stackSlotCount; i++) paramPassingStackSlots[i] = -1;
    }

    void Mov(Location dst, Location src) {
        int value;

        int sidx;
        int didx;

        switch (src.Kind()) {
            case Cbc::Location::IREG: sidx = src.IRegIdx(); value = iregs[sidx]; break;
            case Cbc::Location::FREG: sidx = src.FRegIdx(); value = fregs[sidx]; break;
            case Cbc::Location::SLOT: sidx = src.SlotIdx(); value = untyped[sidx]; break;
            case Cbc::Location::NIL: FATAL("illegal src nil");
        }

        switch (dst.Kind()) {
            case Cbc::Location::IREG: didx = dst.IRegIdx(); iregs[didx] = value; break;
            case Cbc::Location::FREG: didx = dst.FRegIdx(); fregs[didx] = value; break;
            case Cbc::Location::SLOT: didx = dst.SlotIdx(); paramPassingStackSlots[didx] = value; break;
            case Cbc::Location::NIL: FATAL("illegal src nil");
        }
        movCount++;
        if (log) {
            printf("%d(%d:%c) := %d(%d:%c)\n", dst.idx, didx, dst.Kind(), src.idx, sidx, src.Kind());
        }
    }

    void Resolve(MoveResolver& mr)
    {
        mr.Resolve([this](Location dst, Location src) {
            this->Mov(dst, src);
        });
    }
};

std::vector<Emit> Resolve(MoveResolver& mr)
{
    std::vector<Emit> emits;
    mr.Resolve([&](Location dst, Location src) {
        emits.emplace_back(dst, src);
    });
    return emits;
}

} // namespace

TEST(MoveResolver, NoConflict)
{
    MoveResolver mr;
    mr.AddMove(Location{1}, Location{2}); // IR1 -> IR2
    mr.AddMove(Location{3}, Location{4}); // IR3 -> IR4

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.iregs[2], irValue(1));
    EXPECT_EQ(r.iregs[4], irValue(3));
}

TEST(MoveResolver, TwoCycleSwap)
{
    MoveResolver mr;
    mr.AddMove(Location{1}, Location{2}); // IR1 -> IR2
    mr.AddMove(Location{2}, Location{1}); // IR2 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.iregs[2], irValue(1));
    EXPECT_EQ(r.iregs[1], irValue(2));
}

TEST(MoveResolver, ThreeCycle)
{
    MoveResolver mr;
    mr.AddMove(Location{1}, Location{2}); // IR1 -> IR2
    mr.AddMove(Location{2}, Location{3}); // IR2 -> IR3
    mr.AddMove(Location{3}, Location{1}); // IR3 -> IR1

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 4);
    EXPECT_EQ(r.iregs[2], irValue(1));
    EXPECT_EQ(r.iregs[3], irValue(2));
    EXPECT_EQ(r.iregs[1], irValue(3));
}

TEST(MoveResolver, FRegCycle)
{
    MoveResolver mr;
    mr.AddMove(Location{14}, Location{15}); // FR0 -> FR1
    mr.AddMove(Location{15}, Location{14}); // FR1 -> FR0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 3);
    EXPECT_EQ(r.fregs[1], frValue(0));
    EXPECT_EQ(r.fregs[0], frValue(1));
}

TEST(MoveResolver, SlotCycle)
{
    MoveResolver mr;
    mr.AddMove(Location{30}, Location{31}); // Slot0 -> Slot1
    mr.AddMove(Location{31}, Location{30}); // Slot1 -> Slot0

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 2);
    EXPECT_EQ(r.paramPassingStackSlots[1], stValue(0));
    EXPECT_EQ(r.paramPassingStackSlots[0], stValue(1));
}

TEST(MoveResolver, SingleMove)
{
    MoveResolver mr;
    mr.AddMove(Location{1}, Location{2});

    RegsAndSlots r;
    r.Resolve(mr);

    ASSERT_EQ(r.movCount, 1);
    EXPECT_EQ(r.iregs[2], irValue(1));
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
    mr.AddMove(Location{1}, Location{2});
    r1.Resolve(mr);
    ASSERT_EQ(r1.movCount, 1);
    EXPECT_EQ(r1.iregs[2], irValue(1));

    // Clear and reuse
    mr.Clear();
    RegsAndSlots r2;
    mr.AddMove(Location{5}, Location{6});
    mr.AddMove(Location{6}, Location{5});
    r2.Resolve(mr);
    ASSERT_EQ(r2.movCount, 3); // 2 moves + 1 cycle break
    EXPECT_EQ(r2.iregs[5], irValue(6));
    EXPECT_EQ(r2.iregs[6], irValue(5));
}

TEST(MoveResolver, LocationKind)
{
    EXPECT_EQ(Location{0}.Kind(), Location::IREG);
    EXPECT_EQ(Location{13}.Kind(), Location::IREG);
    EXPECT_EQ(Location{14}.Kind(), Location::FREG);
    EXPECT_EQ(Location{29}.Kind(), Location::FREG);
    EXPECT_EQ(Location{30}.Kind(), Location::SLOT);
    EXPECT_EQ(Location{35}.Kind(), Location::SLOT);

    EXPECT_EQ(Location{14}.FRegIdx(), 0u);
    EXPECT_EQ(Location{29}.FRegIdx(), 15u);
    EXPECT_EQ(Location{30}.SlotIdx(), 0u);
    EXPECT_EQ(Location{35}.SlotIdx(), 5u);
}
