#include <gtest/gtest.h>

#include "cbc/call_stager.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "regs_and_slots.h"
#include "utils/vector.h"

using namespace Cbc;

namespace {

constexpr uint32_t kFrameSize = 0x1000;

struct StagerTest : public testing::Test {
    FrameLayout layout { {}, {}, kFrameSize };
    RegsAndSlots regs;
    CallStager stager { nullptr, regs };

    // Captured before resolution: a source can be another argument's destination.
    Utils::Vector<int> argValues;

    void RecordArgs(Utils::Span<const Location> args)
    {
        argValues.Clear();
        for (auto arg : args) {
            argValues.PushBack(regs.Read(arg).value);
        }
    }

    int ValueAt(Location loc) const
    {
        if (loc.Kind() == Location::SLOT) {
            return regs.paramPassingStackSlots[loc.SlotIdx()].value;
        }
        return regs.Read(loc).value;
    }
};

Location Ir(int idx) { return Location::IReg(static_cast<IReg::Value>(idx)); }

Location Fr(int idx) { return Location::FReg(static_cast<FReg::Value>(idx)); }

Location Slt(int idx) { return Location::Slot(idx); }

ParamInfo Param(CbcTypeKind kind)
{
    ParamInfo p;
    p.kind = kind;
    return p;
}

ParamInfo IntParam() { return Param(CbcTypeKind::I64); }

ParamInfo RefParam() { return Param(CbcTypeKind::REF); }

ParamInfo RecParam() { return Param(CbcTypeKind::REC); }

ParamInfo F64Param() { return Param(CbcTypeKind::F64); }

ParamInfo F32Param() { return Param(CbcTypeKind::F32); }

CallAbiFlags StaticFlags() { return { false, false, Image::MethodAbiKind::STATIC }; }

} // namespace

#define EXPECT_PLACED(i, where)                                                                                        \
    EXPECT_EQ(ValueAt(where), argValues[(i)]) << "argument " << (i) << " not placed in " << #where;

TEST_F(StagerTest, IntArgsLandInParameterRegisters)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(3), Ir(4) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
}

TEST_F(StagerTest, FloatArgsLandInFloatRegisters)
{
    Utils::Vector<ParamInfo> params { F64Param(), F32Param() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Fr(2), Fr(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Fr(0));
    EXPECT_PLACED(1, Fr(1));
}

TEST_F(StagerTest, MixedIntAndFloatArgsKeepIndependentCounters)
{
    Utils::Vector<ParamInfo> params { IntParam(), F64Param(), IntParam(), F32Param() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(7), Fr(2), Ir(8), Fr(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Fr(0));
    EXPECT_PLACED(2, Ir(2));
    EXPECT_PLACED(3, Fr(1));
}

TEST_F(StagerTest, IntArgsOverflowToSuccessiveStackSlots)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(IntParam());
        args.PushBack(Ir(i));
    }

    CallSignature sig { params, {}, false, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(ValueAt(Ir(i + 1)), argValues[i]) << "argument " << i;
    }
    for (int i = 6; i < 9; i++) {
        EXPECT_EQ(ValueAt(Slt(i - 6)), argValues[i]) << "stack argument " << i;
    }
}

TEST_F(StagerTest, FloatArgsOverflowToStackSlots)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(F64Param());
        args.PushBack(Fr(i));
    }

    CallSignature sig { params, {}, false, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(ValueAt(Fr(i)), argValues[i]) << "float argument " << i;
    }
    EXPECT_EQ(ValueAt(Slt(0)), argValues[8]) << "spilled float argument";
}

TEST_F(StagerTest, ArgumentFromUntypedSlotLandsInRegister)
{
    Utils::Vector<ParamInfo> params { IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Slt(2) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
}

TEST_F(StagerTest, SwappedArgumentsAreNotClobbered)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(2), Ir(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
}

TEST_F(StagerTest, RotatedArgumentsAreNotClobbered)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(2), Ir(3), Ir(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
    EXPECT_PLACED(2, Ir(3));
}

TEST_F(StagerTest, RefArgOnStackIsReportedAsParamRefSlot)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 7; i++) {
        params.PushBack(RefParam());
        args.PushBack(Ir(i + 1));
    }

    CallSignature sig { params, {}, false, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ASSERT_EQ(result.paramRefSlots.Size(), 1u);
    EXPECT_EQ(result.paramRefSlots[0], 0u);
    EXPECT_EQ(result.paramRecSlots.Size(), 0u);
}

TEST_F(StagerTest, RecArgOnStackIsReportedAsParamRecSlot)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 7; i++) {
        params.PushBack(RecParam());
        args.PushBack(Ir(i + 1));
    }

    CallSignature sig { params, {}, false, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ASSERT_EQ(result.paramRecSlots.Size(), 1u);
    EXPECT_EQ(result.paramRecSlots[0], 0u);
    EXPECT_EQ(result.paramRefSlots.Size(), 0u);
}

TEST_F(StagerTest, MutReceiverStagesDerivedBasePair)
{
    Utils::Vector<ParamInfo> params { IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(5), Ir(6), Ir(7) };

    RecordArgs(args);
    CallAbiFlags flags { false, false, Image::MethodAbiKind::MUT };
    auto result = stager.StageManaged(args, flags, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(ValueAt(Ir(1)), argValues[0]) << "derived receiver";
    EXPECT_EQ(ValueAt(Ir(2)), argValues[1]) << "base receiver";
    EXPECT_EQ(ValueAt(Ir(3)), argValues[2]) << "first parameter";
    ASSERT_TRUE(result.derived.has_value());
    EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR1));
}

TEST_F(StagerTest, GenericClosureParametersArePassedAsReferences)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(3), Ir(4), Ir(5), Ir(6) };

    RecordArgs(args);
    CallAbiFlags flags { false, true, Image::MethodAbiKind::REF_RECEIVER };
    auto result = stager.StageManaged(args, flags, sig, nullptr, nullptr, /*genericParams=*/true);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
    EXPECT_PLACED(2, Ir(3));
    EXPECT_PLACED(3, Ir(4));
}

TEST_F(StagerTest, OuterTiAndThisTiLocationsAreReported)
{
    Utils::Vector<ParamInfo> params { IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(3), Ir(4), Ir(5) };

    Location outerTiLoc = Location::Nil();
    Location thisTiLoc  = Location::Nil();
    CallAbiFlags flags { false, true, Image::MethodAbiKind::HAS_THIS_TI };
    auto result = stager.StageManaged(args, flags, sig, &outerTiLoc, &thisTiLoc);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(outerTiLoc.Kind(), Location::IREG);
    EXPECT_EQ(thisTiLoc.Kind(), Location::IREG);
    EXPECT_NE(outerTiLoc.IRegIdx(), thisTiLoc.IRegIdx());
}

TEST_F(StagerTest, ManagedArgCountMismatchFails)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(3) };

    auto result = stager.StageManaged(args, StaticFlags(), sig);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.failure.empty());
}

TEST_F(StagerTest, ForeignIntArgsLandInParameterRegisters)
{
    Utils::Vector<ParamInfo> params { IntParam(), IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(3), Ir(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
}

TEST_F(StagerTest, ForeignFloatArgsLandInFloatRegisters)
{
    Utils::Vector<ParamInfo> params { F64Param() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Fr(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, Fr(0));
}

TEST_F(StagerTest, ForeignIntArgsOverflowToStack)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 8; i++) {
        params.PushBack(IntParam());
        args.PushBack(Ir(i + 1));
    }

    CallSignature sig { params, {}, false, 0 };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(ValueAt(Ir(i + 1)), argValues[i]) << "argument " << i;
    }
    EXPECT_EQ(ValueAt(Slt(0)), argValues[6]) << "7th argument";
    EXPECT_EQ(ValueAt(Slt(1)), argValues[7]) << "8th argument";
    EXPECT_EQ(staging.stackSlots, 2);
}

TEST_F(StagerTest, ForeignStackArgFromUntypedSlotLandsInStackSlot)
{
    Utils::Vector<ParamInfo> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 7; i++) {
        params.PushBack(IntParam());
        args.PushBack(Ir(i + 1));
    }
    args[6] = Slt(2);

    CallSignature sig { params, {}, false, 0 };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_EQ(ValueAt(Slt(0)), argValues[6]);
    EXPECT_EQ(staging.stackSlots, 1);
}

TEST_F(StagerTest, ForeignVarArgLabelIsNotAnArgument)
{
    ParamInfo label;
    label.isVarArgLabel = true;

    Utils::Vector<ParamInfo> params { IntParam(), label, IntParam() };
    CallSignature sig { params, {}, /*isVarArg=*/true, 0 };
    Utils::Vector<Location> args { Ir(3), Ir(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, Ir(1));
    EXPECT_PLACED(1, Ir(2));
    EXPECT_EQ(staging.stackSlots, 0);
}

TEST_F(StagerTest, ForeignVArrayArgIsPassedByReference)
{
    ParamInfo varray;
    varray.isVArray = true;

    Utils::Vector<ParamInfo> params { varray };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, Ir(1));
}

TEST_F(StagerTest, ForeignArgCountMismatchFails)
{
    Utils::Vector<ParamInfo> params { IntParam() };
    CallSignature sig { params, {}, false, 0 };
    Utils::Vector<Location> args { Ir(1), Ir(2) };

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    EXPECT_FALSE(staging.result.ok);
    EXPECT_FALSE(staging.result.failure.empty());
}
