#include <gtest/gtest.h>

#include "cbc/call_stager.h"
#include "cbc/isa.h"
#include "regs_and_slots.h"
#include "utils/vector.h"

using namespace Cbc;

namespace {

static AbiAssigner::PlatformDescription DescriptionFor(Platform p)
{
    switch (p) {
        case Platform::LINUX_X64:      return AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_X64>();
        case Platform::LINUX_AARCH64:  return AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_AARCH64>();
        case Platform::DARWIN_AARCH64: return AbiAssigner::PlatformDescription::FromTraits<Platform::DARWIN_AARCH64>();
    }
    FATAL("unknown platform");
}

struct StagerTest : public testing::TestWithParam<Platform> {
    AbiAssigner::PlatformDescription desc = DescriptionFor(GetParam());
    RegsAndSlots sink;
    CallStager stager { sink, desc };

    void TearDown() override { EXPECT_FALSE(sink.overlappingWrites); }

    CAbi CAbiOf() const { return desc.cAbi; }

    int IntRegCount() const { return static_cast<int>(desc.iregHeadArea.Size()); }

    int FloatRegCount() const { return static_cast<int>(desc.fregHeadArea.Size()); }

    Utils::Vector<int> argValues;

    void RecordArgs(Utils::Span<const Location> args)
    {
        argValues.Clear();
        for (auto arg : args) {
            argValues.PushBack(sink.Read(arg).value);
        }
    }

    void ExpectPlaced(size_t index, Location target) const
    {
        ASSERT_TRUE(target.Kind() == Location::IREG || target.Kind() == Location::FREG);
        EXPECT_EQ(sink.Read(target).value, argValues[index]) << "argument " << index << ", target " << target.idx;
    }

    void ExpectStack(uint32_t offset, Format::StoreAccessKind stk, Cell cell) const
    {
        ASSERT_LT(offset, static_cast<uint32_t>(RegsAndSlots::stackByteCount));
        EXPECT_EQ(sink.paramBytes[offset], cell) << "stack offset " << offset;
        EXPECT_EQ(sink.storeKinds[offset], static_cast<uint8_t>(stk)) << "stack offset " << offset;
    }
};

Location IRegLoc(int idx) { return Location::IReg(static_cast<IReg::Value>(idx)); }

Location FRegLoc(int idx) { return Location::FReg(static_cast<FReg::Value>(idx)); }

Location SlotLoc(int idx) { return Location::Slot(idx); }

ForeignParam FlatRecParam(std::initializer_list<CAbiField> leaves, uint32_t size)
{
    ForeignParam p { CbcTypeKind::REC };
    for (auto leaf : leaves) {
        p.fields.PushBack(leaf);
    }
    p.flatSize = size;
    return p;
}

CallAbiFlags StaticFlags() { return { false, false, Image::MethodAbiKind::STATIC }; }

} // namespace

TEST_P(StagerTest, MixedIntAndFloatArgsKeepIndependentCounters)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64, CbcTypeKind::F64, CbcTypeKind::I64, CbcTypeKind::F32 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(7), FRegLoc(2), IRegLoc(8), FRegLoc(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, FRegLoc(0));
    ExpectPlaced(2, IRegLoc(2));
    ExpectPlaced(3, FRegLoc(1));
}

TEST_P(StagerTest, IntArgsOverflowToSuccessiveStackSlots)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    int regs = IntRegCount();
    for (int i = 0; i < regs; i++) {
        ExpectPlaced(i, IRegLoc(i + 1));
    }
    for (int i = regs; i < 9; i++) {
        ExpectStack((i - regs) * STACK_SLOT_SIZE, Format::StoreAccessKind::ST_64, Cell::Move(argValues[i]));
    }
}

TEST_P(StagerTest, FloatArgsOverflowToStackSlots)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(CbcTypeKind::F64);
        args.PushBack(FRegLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    int regs = FloatRegCount();
    for (int i = 0; i < regs; i++) {
        ExpectPlaced(i, FRegLoc(i));
    }
    ExpectStack(0, Format::StoreAccessKind::ST_F64, Cell::Move(argValues[regs]));
}

TEST_P(StagerTest, ArgumentFromUntypedSlotLandsInRegister)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(2) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
}

TEST_P(StagerTest, SwappedArgumentsAreNotClobbered)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64, CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(2), IRegLoc(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
}

TEST_P(StagerTest, RotatedArgumentsAreNotClobbered)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64, CbcTypeKind::I64, CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(2), IRegLoc(3), IRegLoc(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
    ExpectPlaced(2, IRegLoc(3));
}

TEST_P(StagerTest, RefArgOnStackIsReportedAsParamRefSlot)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i <= IntRegCount(); i++) {
        params.PushBack(CbcTypeKind::REF);
        args.PushBack(IRegLoc(i + 1));
    }

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ASSERT_EQ(result.paramRefSlots.Size(), 1u);
    EXPECT_EQ(result.paramRefSlots[0], 0u);
    EXPECT_EQ(result.paramRecSlots.Size(), 0u);
}

TEST_P(StagerTest, RecArgOnStackIsReportedAsParamRecSlot)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i <= IntRegCount(); i++) {
        params.PushBack(CbcTypeKind::REC);
        args.PushBack(IRegLoc(i + 1));
    }

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ASSERT_EQ(result.paramRecSlots.Size(), 1u);
    EXPECT_EQ(result.paramRecSlots[0], 0u);
    EXPECT_EQ(result.paramRefSlots.Size(), 0u);
}

TEST_P(StagerTest, MutReceiverStagesDerivedBasePair)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6), IRegLoc(7) };

    RecordArgs(args);
    CallAbiFlags flags { false, false, Image::MethodAbiKind::MUT };
    auto result = stager.StageManaged(args, flags, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
    ExpectPlaced(2, IRegLoc(3));
    ASSERT_TRUE(result.derived.has_value());
    EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR1));
}

TEST_P(StagerTest, OuterTiAndThisTiLocationsAreReported)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5) };

    RecordArgs(args);
    Location outerTiLoc = Location::Nil();
    Location thisTiLoc  = Location::Nil();
    CallAbiFlags flags { false, true, Image::MethodAbiKind::HAS_THIS_TI };
    auto result = stager.StageManaged(args, flags, sig, &outerTiLoc, &thisTiLoc);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(outerTiLoc, IRegLoc(2));
    EXPECT_EQ(thisTiLoc, IRegLoc(3));
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, outerTiLoc);
    ExpectPlaced(2, thisTiLoc);
}

TEST_P(StagerTest, ManagedArgCountMismatchFails)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64, CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3) };

    auto result = stager.StageManaged(args, StaticFlags(), sig);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.failure.empty());
}

TEST_P(StagerTest, ForeignMixedArgsKeepFpRegistersWhenIntegersOverflow)
{
    int regs = IntRegCount();
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs + 2; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    params.PushBack(CbcTypeKind::F64);
    args.PushBack(FRegLoc(4));
    params.PushBack(CbcTypeKind::F32);
    args.PushBack(FRegLoc(5));

    ForeignCallSignature sig { params };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    for (int i = 0; i < regs; i++) {
        ExpectPlaced(i, IRegLoc(i + 1));
    }
    ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[regs]));
    ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[regs + 1]));
    ExpectPlaced(regs + 2, FRegLoc(0));
    ExpectPlaced(regs + 3, FRegLoc(1));
    EXPECT_EQ(staging.stackBytes, 2 * Cbc::STACK_SLOT_SIZE);
}

TEST_P(StagerTest, ForeignStackArgFromUntypedSlotLandsInStackSlot)
{
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    int spillIdx = IntRegCount();
    for (int i = 0; i <= spillIdx; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    args[spillIdx] = SlotLoc(2);

    ForeignCallSignature sig { params };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[spillIdx]));
    EXPECT_EQ(staging.stackBytes, Cbc::STACK_SLOT_SIZE);
}

TEST_P(StagerTest, ForeignVarArgLabelIsNotAnArgument)
{
    ForeignParam label;
    label.isVarArgLabel = true;

    Utils::Vector<ForeignParam> params { CbcTypeKind::I64, label, CbcTypeKind::I64 };
    ForeignCallSignature sig { params, /*isVarArg=*/true };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(0, IRegLoc(1));
    switch (CAbiOf()) {
        case CAbi::SYSV:
        case CAbi::AAPCS64:        ExpectPlaced(1, IRegLoc(2)); break;
        case CAbi::DARWIN_AARCH64: ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[1])); break;
    }
}

TEST_P(StagerTest, ForeignVArrayArgIsPassedByReference)
{
    ForeignParam varray;
    varray.isVArray = true;

    Utils::Vector<ForeignParam> params { varray };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { IRegLoc(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(0, IRegLoc(1));
}

TEST_P(StagerTest, ForeignArgCountMismatchFails)
{
    Utils::Vector<ForeignParam> params { CbcTypeKind::I64 };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { IRegLoc(1), IRegLoc(2) };

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    EXPECT_FALSE(staging.result.ok);
    EXPECT_FALSE(staging.result.failure.empty());
}

TEST_P(StagerTest, VariadicVArrayPointerAndFollowingScalar)
{
    ForeignParam label;
    label.isVarArgLabel = true;
    ForeignParam varray;
    varray.isVArray = true;
    ForeignCallSignature sig { { CbcTypeKind::I64, label, varray, CbcTypeKind::I64 }, true };
    Utils::Vector<Location> args { IRegLoc(3), SlotLoc(0), IRegLoc(4) };
    RecordArgs(args);

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(0, IRegLoc(1));
    if (GetParam() == Platform::DARWIN_AARCH64) {
        ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[1]));
        ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[2]));
        EXPECT_EQ(staging.stackBytes, 16);
    } else {
        ExpectPlaced(1, IRegLoc(2));
        ExpectPlaced(2, IRegLoc(3));
        EXPECT_EQ(staging.stackBytes, 0);
    }
}

TEST_P(StagerTest, VariadicLargeStructCopyAndFollowingScalar)
{
    ForeignParam label;
    label.isVarArgLabel = true;
    auto record         = FlatRecParam({ { 0, 8, false }, { 8, 8, false }, { 16, 8, false } }, 24);
    ForeignCallSignature sig { { CbcTypeKind::I64, label, record, CbcTypeKind::I64 }, true };
    Utils::Vector<Location> args { IRegLoc(3), SlotLoc(0), IRegLoc(4) };
    RecordArgs(args);

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(0, IRegLoc(1));
    uint32_t copyOffset = 0;
    switch (GetParam()) {
        case Platform::LINUX_X64:
            ExpectPlaced(2, IRegLoc(2));
            EXPECT_EQ(staging.stackBytes, 24);
            EXPECT_EQ(staging.result.maxParamPassingSize, 24u);
            break;
        case Platform::LINUX_AARCH64:
            EXPECT_EQ(sink.iregs[2], Cell::ParamAddress(0));
            ExpectPlaced(2, IRegLoc(3));
            EXPECT_EQ(staging.stackBytes, 0);
            EXPECT_EQ(staging.result.maxParamPassingSize, 24u);
            break;
        case Platform::DARWIN_AARCH64:
            copyOffset = 16;
            ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::ParamAddress(copyOffset));
            ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[2]));
            EXPECT_EQ(staging.stackBytes, 16);
            EXPECT_EQ(staging.result.maxParamPassingSize, 40u);
            break;
    }
    for (uint32_t offset = 0; offset < 24; offset += STACK_SLOT_SIZE) {
        ExpectStack(
            copyOffset + offset,
            Format::StoreAccessKind::ST_64,
            Cell::Load(argValues[1], offset, Format::LoadAccessKind::LD_64)
        );
    }
}

TEST_P(StagerTest, SpilledHfaExhaustsRemainingFpRegisters)
{
    ForeignCallSignature sig;
    Utils::Vector<Location> args;
    for (int i = 0; i < 7; i++) {
        sig.params.PushBack(CbcTypeKind::F64);
        args.PushBack(FRegLoc(i));
    }
    sig.params.PushBack(FlatRecParam({ { 0, 4, true }, { 4, 4, true } }, 8));
    args.PushBack(SlotLoc(0));
    sig.params.PushBack(CbcTypeKind::F32);
    args.PushBack(FRegLoc(9));
    RecordArgs(args);

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    for (int i = 0; i < 7; i++) {
        ExpectPlaced(i, FRegLoc(i));
    }
    EXPECT_EQ(staging.fregs, 8);
    if (GetParam() == Platform::LINUX_X64) {
        EXPECT_EQ(sink.fregs[7], Cell::Load(argValues[7], 0, Format::LoadAccessKind::LD_F64));
        ExpectStack(0, Format::StoreAccessKind::ST_F32, Cell::Move(argValues[8]));
        EXPECT_EQ(staging.stackBytes, 8);
    } else {
        ExpectStack(0, Format::StoreAccessKind::ST_32, Cell::Load(argValues[7], 0, Format::LoadAccessKind::LD_32));
        ExpectStack(4, Format::StoreAccessKind::ST_32, Cell::Load(argValues[7], 4, Format::LoadAccessKind::LD_32));
        ExpectStack(8, Format::StoreAccessKind::ST_F32, Cell::Move(argValues[8]));
        EXPECT_EQ(staging.stackBytes, 16);
    }
    EXPECT_EQ(staging.result.maxParamPassingSize, static_cast<uint32_t>(staging.stackBytes));
}

TEST_P(StagerTest, SpilledHfaRespectsAlignmentAndTrailingArgument)
{
    ForeignCallSignature sig;
    Utils::Vector<Location> args;
    for (int i = 0; i < IntRegCount(); i++) {
        sig.params.PushBack(CbcTypeKind::I64);
        args.PushBack(SlotLoc(i));
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        sig.params.PushBack(CbcTypeKind::F64);
        args.PushBack(FRegLoc(i));
    }
    size_t prefixIndex = args.Size();
    sig.params.PushBack(CbcTypeKind::I8);
    args.PushBack(IRegLoc(11));
    sig.params.PushBack(FlatRecParam({ { 0, 4, true }, { 4, 4, true }, { 8, 4, true } }, 12));
    args.PushBack(SlotLoc(20));
    sig.params.PushBack(CbcTypeKind::F32);
    args.PushBack(FRegLoc(9));
    RecordArgs(args);

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectStack(0, Format::StoreAccessKind::ST_8, Cell::Move(argValues[prefixIndex]));
    uint32_t base = GetParam() == Platform::DARWIN_AARCH64 ? 4 : 8;
    if (GetParam() == Platform::LINUX_X64) {
        for (uint32_t offset = 0; offset < 16; offset += STACK_SLOT_SIZE) {
            ExpectStack(
                base + offset,
                Format::StoreAccessKind::ST_64,
                Cell::Load(argValues[prefixIndex + 1], offset, Format::LoadAccessKind::LD_64)
            );
        }
    } else {
        for (uint32_t offset = 0; offset < 12; offset += 4) {
            ExpectStack(
                base + offset,
                Format::StoreAccessKind::ST_32,
                Cell::Load(argValues[prefixIndex + 1], offset, Format::LoadAccessKind::LD_32)
            );
        }
    }
    uint32_t markerOffset = GetParam() == Platform::DARWIN_AARCH64 ? 16 : 24;
    ExpectStack(markerOffset, Format::StoreAccessKind::ST_F32, Cell::Move(argValues[prefixIndex + 2]));
    EXPECT_EQ(staging.stackBytes, GetParam() == Platform::DARWIN_AARCH64 ? 24 : 32);
}

TEST_P(StagerTest, ForeignRegisterReturnPreservesSretAddressAcrossShuffle)
{
    ForeignCallSignature sig { { CbcTypeKind::I64, CbcTypeKind::I64 } };
    sig.retFields.PushBack({ 0, 8, false });
    sig.retFields.PushBack({ 8, 8, false });
    sig.retSize = 16;
    CallAbiFlags flags { true, false, Image::MethodAbiKind::STATIC };
    Utils::Vector<Location> args { IRegLoc(1), IRegLoc(2), IRegLoc(3) };
    RecordArgs(args);
    auto plan = stager.StageForeignStructRet(sig, flags);
    ASSERT_TRUE(plan.has_value());
    ASSERT_EQ(plan->passBy, CAbiPassBy::REGS);
    EXPECT_EQ(plan->PiecesNum(), 2);

    auto staging = stager.StageForeign(args, flags, sig, plan);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(1, IRegLoc(1));
    ExpectPlaced(2, IRegLoc(2));
    EXPECT_EQ(sink.iregs[IReg::IR_ACC], Cell::Move(argValues[0]));
    ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[0]));
    EXPECT_EQ(staging.result.maxParamPassingSize, 8u);
}

TEST_P(StagerTest, ForeignIndirectReturnUsesSretLocation)
{
    ForeignCallSignature sig { { CbcTypeKind::I64 } };
    sig.retFields.PushBack({ 0, 8, false });
    sig.retFields.PushBack({ 8, 8, false });
    sig.retFields.PushBack({ 16, 8, false });
    sig.retSize = 24;
    CallAbiFlags flags { true, false, Image::MethodAbiKind::STATIC };
    Utils::Vector<Location> args { SlotLoc(0), IRegLoc(3) };
    RecordArgs(args);
    auto plan = stager.StageForeignStructRet(sig, flags);
    ASSERT_TRUE(plan.has_value());
    ASSERT_NE(plan->passBy, CAbiPassBy::REGS);

    auto staging = stager.StageForeign(args, flags, sig, plan);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    ExpectPlaced(0, IRegLoc(desc.sretShifts ? 1 : desc.sretIrIdx));
    ExpectPlaced(1, IRegLoc(desc.sretShifts ? 2 : 1));
    EXPECT_EQ(staging.stackBytes, 0);
    EXPECT_EQ(staging.result.maxParamPassingSize, 0u);
}

TEST_P(StagerTest, HiddenTypeInfosSpillAfterExplicitParameters)
{
    ManagedCallSignature sig;
    Utils::Vector<Location> args;
    for (int i = 0; i < IntRegCount(); i++) {
        sig.params.PushBack(CbcTypeKind::I64);
        args.PushBack(SlotLoc(i));
    }
    args.PushBack(IRegLoc(11));
    args.PushBack(IRegLoc(12));
    RecordArgs(args);
    Location outerTi = Location::Nil();
    Location thisTi  = Location::Nil();

    auto result = stager.StageManaged(args, { false, true, Image::MethodAbiKind::HAS_THIS_TI }, sig, &outerTi, &thisTi);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(outerTi, SlotLoc(0));
    EXPECT_EQ(thisTi, SlotLoc(1));
    ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[IntRegCount()]));
    ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[IntRegCount() + 1]));
    EXPECT_EQ(result.maxParamPassingSize, 16u);
}

TEST_P(StagerTest, ForeignSmallStructIsPassedInIntegerRegisters)
{
    Utils::Vector<ForeignParam> params { FlatRecParam({ { 0, 8, false }, { 8, 8, false } }, 16) };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { SlotLoc(0) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_EQ(staging.stackBytes, 0);
    EXPECT_EQ(sink.iregs[1], Cell::Load(argValues[0], 0, Format::LoadAccessKind::LD_64));
    EXPECT_EQ(sink.iregs[2], Cell::Load(argValues[0], 8, Format::LoadAccessKind::LD_64));
}

TEST_P(StagerTest, ForeignLargeStructAbove16Bytes)
{
    Utils::Vector<ForeignParam> params {
        FlatRecParam({ { 0, 8, false }, { 8, 8, false }, { 16, 8, false }, { 24, 8, false } }, 32)
    };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { SlotLoc(0) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    switch (CAbiOf()) {
        case CAbi::SYSV:
            // Copied onto the stack, one slot per 8 bytes.
            EXPECT_EQ(staging.stackBytes, 4 * Cbc::STACK_SLOT_SIZE);
            break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64:
            // Passed as a pointer to a caller-made copy in IR1.
            EXPECT_EQ(staging.stackBytes, 0);
            EXPECT_EQ(sink.iregs[1], Cell::ParamAddress(0));
            break;
    }
    for (uint32_t offset = 0; offset < 32; offset += STACK_SLOT_SIZE) {
        ExpectStack(
            offset, Format::StoreAccessKind::ST_64, Cell::Load(argValues[0], offset, Format::LoadAccessKind::LD_64)
        );
    }
    EXPECT_EQ(staging.result.maxParamPassingSize, 32u);
}

TEST_P(StagerTest, ForeignAllFloatStruct)
{
    Utils::Vector<ForeignParam> params {
        FlatRecParam({ { 0, 4, true }, { 4, 4, true }, { 8, 4, true }, { 12, 4, true } }, 16)
    };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { SlotLoc(0) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    switch (CAbiOf()) {
        case CAbi::SYSV:
            // Each 8-byte chunk is one piece: two F64 registers.
            EXPECT_EQ(staging.fregs, 2);
            EXPECT_EQ(sink.fregs[0], Cell::Load(argValues[0], 0, Format::LoadAccessKind::LD_F64));
            EXPECT_EQ(sink.fregs[1], Cell::Load(argValues[0], 8, Format::LoadAccessKind::LD_F64));
            break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64:
            // HFA: one FP register per element.
            EXPECT_EQ(staging.fregs, 4);
            for (uint32_t i = 0; i < 4; i++) {
                EXPECT_EQ(sink.fregs[i], Cell::Load(argValues[0], i * 4, Format::LoadAccessKind::LD_F32));
            }
            break;
    }
}

TEST_P(StagerTest, ForeignStructRetIsSkippedWhenNotSRet)
{
    ForeignCallSignature sig;
    sig.retFields.PushBack({ 0, 8, false });
    sig.retSize = 8;

    auto plan = stager.StageForeignStructRet(sig, StaticFlags());

    EXPECT_FALSE(plan.has_value());
}

TEST_P(StagerTest, ManagedSRetOccupiesDedicatedOrShiftedSlot)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(4), IRegLoc(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { true, false, Image::MethodAbiKind::STATIC }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
            // sret takes IR1 as a record, so the parameter is shifted to IR2.
            ExpectPlaced(0, IRegLoc(1));
            ExpectPlaced(1, IRegLoc(2));
            break;
        case Platform::LINUX_AARCH64:
        case Platform::DARWIN_AARCH64:
            // sret goes to a dedicated register, parameters start at IR1.
            ExpectPlaced(1, IRegLoc(1));
            ExpectPlaced(0, IRegLoc(9));
            break;
    }
}

TEST_P(StagerTest, ForeignStructRetUsesPlatformFpBudget)
{
    ForeignCallSignature sig;
    sig.retFields = { { 0, 4, true }, { 4, 4, true }, { 8, 4, true }, { 12, 4, true } };
    sig.retSize   = 16;

    auto plan = stager.StageForeignStructRet(sig, { true, false, Image::MethodAbiKind::STATIC });

    ASSERT_TRUE(plan.has_value());
    ASSERT_EQ(plan->passBy, CAbiPassBy::REGS);
    switch (CAbiOf()) {
        case CAbi::SYSV:           EXPECT_EQ(plan->PiecesNum(), 2); break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64: EXPECT_EQ(plan->PiecesNum(), 4); break;
    }
}

TEST_P(StagerTest, PrimReceiverTakesIntegerRegister)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::PRIM_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
}

TEST_P(StagerTest, FPrimReceiverTakesFloatRegister)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { FRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::FPRIM_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, FRegLoc(0));
    ExpectPlaced(1, IRegLoc(1));
}

TEST_P(StagerTest, RecReceiverTakesIntegerRegister)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::REC_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
}

TEST_P(StagerTest, FuncTypeVariablesTakeIntegerRegisters)
{
    // Generic instance: type vars are passed as trailing integer arguments.
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, /*funcVarCount=*/2 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectPlaced(0, IRegLoc(1));
    ExpectPlaced(1, IRegLoc(2));
    ExpectPlaced(2, IRegLoc(3));
}

TEST_P(StagerTest, FuncTypeVariablesOverflowToStackAfterParams)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    // Two type vars cannot fit, they spill in order after the parameters.
    args.PushBack(IRegLoc(regs + 1));
    args.PushBack(IRegLoc(regs + 2));

    ManagedCallSignature sig { params, 2 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ExpectStack(0, Format::StoreAccessKind::ST_64, Cell::Move(argValues[regs]));
    ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[regs + 1]));
}

TEST_P(StagerTest, GenericParamsIgnoreDeclaredKindsAndPassReferences)
{
    // Generic closure: every parameter is passed as a managed reference,
    // regardless of the declared kind.
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::REC, CbcTypeKind::F64 };
    ManagedCallSignature sig { params, 0 };
    // Receiver, two parameters, outer type info.
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(
        args, { false, true, Image::MethodAbiKind::REF_RECEIVER }, sig, nullptr, nullptr, /*genericParams=*/true
    );

    ASSERT_TRUE(result.ok) << result.failure;
    for (int i = 0; i < 4; i++) {
        ExpectPlaced(i, IRegLoc(i + 1));
    }
}

TEST_P(StagerTest, MutReceiverWithSRet)
{
    Utils::Vector<CbcTypeKind> params { CbcTypeKind::I64 };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(4), IRegLoc(5), IRegLoc(6), IRegLoc(7) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { true, false, Image::MethodAbiKind::MUT }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    ASSERT_TRUE(result.derived.has_value());
    switch (GetParam()) {
        case Platform::LINUX_X64:
            // sret shifts everything: sret, derived, base, param.
            ExpectPlaced(0, IRegLoc(1));
            ExpectPlaced(1, IRegLoc(2));
            ExpectPlaced(2, IRegLoc(3));
            ExpectPlaced(3, IRegLoc(4));
            EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR2));
            break;
        case Platform::LINUX_AARCH64:
        case Platform::DARWIN_AARCH64:
            // Dedicated sret register: derived, base, param start at IR1.
            ExpectPlaced(0, IRegLoc(9));
            ExpectPlaced(1, IRegLoc(1));
            ExpectPlaced(2, IRegLoc(2));
            ExpectPlaced(3, IRegLoc(3));
            EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR1));
            break;
    }
}

TEST_P(StagerTest, CompactSlotsFitSmallArgs)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    for (int i = 0; i < 4; i++) {
        params.PushBack(CbcTypeKind::I16);
        args.PushBack(SlotLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    for (uint32_t i = 0; i < 4; i++) {
        auto offset = GetParam() == Platform::DARWIN_AARCH64 ? i * 2 : i * STACK_SLOT_SIZE;
        ExpectStack(offset, Format::StoreAccessKind::ST_16, Cell::Move(argValues[regs + i]));
    }
    EXPECT_EQ(result.maxParamPassingSize, GetParam() == Platform::DARWIN_AARCH64 ? 8u : 32u);
}

TEST_P(StagerTest, CompactSlotsPackMixedWidths)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    params.PushBack(CbcTypeKind::BOOL);
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I32);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I16);
    args.PushBack(SlotLoc(0));
    args.PushBack(SlotLoc(1));
    args.PushBack(SlotLoc(2));
    args.PushBack(SlotLoc(3));
    args.PushBack(SlotLoc(4));

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    uint32_t darwinOffsets[]        = { 0, 1, 4, 8, 10 };
    Format::StoreAccessKind kinds[] = {
        Format::StoreAccessKind::ST_8,  Format::StoreAccessKind::ST_8,  Format::StoreAccessKind::ST_32,
        Format::StoreAccessKind::ST_16, Format::StoreAccessKind::ST_16,
    };
    for (uint32_t i = 0; i < 5; i++) {
        auto offset = GetParam() == Platform::DARWIN_AARCH64 ? darwinOffsets[i] : i * STACK_SLOT_SIZE;
        ExpectStack(offset, kinds[i], Cell::Move(argValues[regs + i]));
    }
    EXPECT_EQ(result.maxParamPassingSize, GetParam() == Platform::DARWIN_AARCH64 ? 16u : 40u);
}

TEST_P(StagerTest, CompactSlotsForForeignNamedArgs)
{
    int nRegs = IntRegCount();
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < nRegs; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    for (int i = 0; i < 4; i++) {
        params.PushBack(CbcTypeKind::I16);
        args.PushBack(SlotLoc(i));
    }

    ForeignCallSignature sig { params };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    for (uint32_t i = 0; i < 4; i++) {
        auto offset = GetParam() == Platform::DARWIN_AARCH64 ? i * 2 : i * STACK_SLOT_SIZE;
        ExpectStack(offset, Format::StoreAccessKind::ST_16, Cell::Move(argValues[nRegs + i]));
    }
    EXPECT_EQ(staging.stackBytes, GetParam() == Platform::DARWIN_AARCH64 ? 8 : 32);
}

TEST_P(StagerTest, CompactSlotsRespectNaturalAlignment)
{
    int nRegs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < nRegs; i++) {
        params.PushBack(CbcTypeKind::I64);
        args.PushBack(IRegLoc(i + 1));
    }
    // A 1-byte argument followed by a 64-bit one: the latter must be aligned,
    // not packed at offset 1.
    params.PushBack(CbcTypeKind::BOOL);
    params.PushBack(CbcTypeKind::I64);
    args.PushBack(SlotLoc(0));
    args.PushBack(SlotLoc(1));

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(result.maxParamPassingSize, 2 * Cbc::STACK_SLOT_SIZE);
    ExpectStack(0, Format::StoreAccessKind::ST_8, Cell::Move(argValues[nRegs]));
    ExpectStack(8, Format::StoreAccessKind::ST_64, Cell::Move(argValues[nRegs + 1]));
}

TEST_P(StagerTest, TailOffsetsPackPerPlatform)
{
    Utils::Vector<CbcTypeKind> params;
    for (int i = 0; i < IntRegCount(); i++) {
        params.PushBack(CbcTypeKind::I64);
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(CbcTypeKind::F64);
    }
    // Every following parameter overflows to the passing area.
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I64);
    params.PushBack(CbcTypeKind::F32);
    params.PushBack(CbcTypeKind::I16);

    ManagedCallSignature sig { params, 0 };
    auto tail = ComputeTailParams(sig, StaticFlags(), desc);

    ASSERT_EQ(tail.Size(), 5u);
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64:
            EXPECT_EQ(tail[0].offset, 0u);
            EXPECT_EQ(tail[1].offset, 8u);
            EXPECT_EQ(tail[2].offset, 16u);
            EXPECT_EQ(tail[3].offset, 24u);
            EXPECT_EQ(tail[4].offset, 32u);
            break;
        case Platform::DARWIN_AARCH64:
            EXPECT_EQ(tail[0].offset, 0u);
            EXPECT_EQ(tail[1].offset, 1u);
            EXPECT_EQ(tail[2].offset, 8u);  // 64-bit value aligned to 8
            EXPECT_EQ(tail[3].offset, 16u); // f32 aligned to 4
            EXPECT_EQ(tail[4].offset, 20u); // i16 aligned to 2
            break;
    }
}

TEST_P(StagerTest, TailLoadKindsFollowParameterKinds)
{
    Utils::Vector<CbcTypeKind> params;
    for (int i = 0; i < IntRegCount(); i++) {
        params.PushBack(CbcTypeKind::I64);
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(CbcTypeKind::F64);
    }
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I8);
    params.PushBack(CbcTypeKind::U16);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I32);
    params.PushBack(CbcTypeKind::F32);
    params.PushBack(CbcTypeKind::I64);
    params.PushBack(CbcTypeKind::REF);
    params.PushBack(CbcTypeKind::REC);

    ManagedCallSignature sig { params, 0 };
    auto tail = ComputeTailParams(sig, StaticFlags(), desc);

    ASSERT_EQ(tail.Size(), 9u);
    // Short integrals are widened to 32-bit in the bytecode (`ld.tail.32`) but
    // occupy their natural width in the passing area, so the load kind comes
    // from the parameter kind. Records arrive as pointers and load as words.
    EXPECT_EQ(tail[0].ldk, Format::LoadAccessKind::LD_U8);
    EXPECT_EQ(tail[1].ldk, Format::LoadAccessKind::LD_S8);
    EXPECT_EQ(tail[2].ldk, Format::LoadAccessKind::LD_U16);
    EXPECT_EQ(tail[3].ldk, Format::LoadAccessKind::LD_S16);
    EXPECT_EQ(tail[4].ldk, Format::LoadAccessKind::LD_32);
    EXPECT_EQ(tail[5].ldk, Format::LoadAccessKind::LD_F32);
    EXPECT_EQ(tail[6].ldk, Format::LoadAccessKind::LD_64);
    EXPECT_EQ(tail[7].ldk, Format::LoadAccessKind::LD_REF);
    EXPECT_EQ(tail[8].ldk, Format::LoadAccessKind::LD_64);
}

TEST_P(StagerTest, TailParamsMatchManagedStaging)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < IntRegCount(); i++) {
        params.PushBack(CbcTypeKind::I64);
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(CbcTypeKind::F64);
    }
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I64);
    params.PushBack(CbcTypeKind::F32);
    params.PushBack(CbcTypeKind::REF);
    params.PushBack(CbcTypeKind::REC);
    for (size_t i = 0; i < params.Size(); i++) {
        args.PushBack(SlotLoc(static_cast<int>(i)));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);
    ASSERT_TRUE(result.ok) << result.failure;

    auto tail = ComputeTailParams(sig, StaticFlags(), desc);

    ASSERT_EQ(tail.Size(), 6u);
    uint32_t darwinOffsets[] = { 0, 2, 8, 16, 24, 32 };
    Format::StoreAccessKind storeKinds[] = {
        Format::StoreAccessKind::ST_8,   Format::StoreAccessKind::ST_16, Format::StoreAccessKind::ST_64,
        Format::StoreAccessKind::ST_F32, Format::StoreAccessKind::ST_64, Format::StoreAccessKind::ST_64,
    };
    size_t firstStackArg     = IntRegCount() + FloatRegCount();
    for (size_t i = 0; i < tail.Size(); i++) {
        uint32_t offset = GetParam() == Platform::DARWIN_AARCH64 ? darwinOffsets[i] : i * STACK_SLOT_SIZE;
        EXPECT_EQ(tail[i].offset, offset) << "tail ordinal " << i;
        ExpectStack(offset, storeKinds[i], Cell::Move(argValues[firstStackArg + i]));
    }
}

TEST_P(StagerTest, PassingAreaBeyondSixteenSlots)
{
    ManagedCallSignature sig;
    Utils::Vector<Location> args;
    for (int i = 0; i < IntRegCount() + 20; i++) {
        sig.params.PushBack(CbcTypeKind::I64);
        args.PushBack(SlotLoc(i));
    }
    RecordArgs(args);

    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(result.maxParamPassingSize, 20u * STACK_SLOT_SIZE);
    for (uint32_t i = 0; i < 20; i++) {
        ExpectStack(i * STACK_SLOT_SIZE, Format::StoreAccessKind::ST_64, Cell::Move(argValues[IntRegCount() + i]));
    }
}

TEST(RegsAndSlots, AddressLoadsKeepByteOffsetAndStoreWidth)
{
    RegsAndSlots sink;
    sink.AssignSlotFromAddr(16, SlotLoc(0), 12, Format::LoadAccessKind::LD_32, 4);
    auto index = 16 * STACK_SLOT_SIZE + 4;
    EXPECT_EQ(sink.paramBytes[index], Cell::Load(StValue(0), 12, Format::LoadAccessKind::LD_32));
    EXPECT_EQ(sink.storeKinds[index], static_cast<uint8_t>(Format::StoreAccessKind::ST_32));
    EXPECT_TRUE(sink.writtenBytes[index + 3]);
    EXPECT_FALSE(sink.writtenBytes[index + 4]);
    EXPECT_FALSE(sink.overlappingWrites);
}

TEST(RegsAndSlots, OverlappingWritesAreDetected)
{
    RegsAndSlots sink;
    sink.AssignSlotFromIReg(0, IReg::IR1, Format::StoreAccessKind::ST_64, 0);
    sink.AssignSlotFromFReg(0, FReg::FR0, Format::StoreAccessKind::ST_F32, 4);
    EXPECT_TRUE(sink.overlappingWrites);
}

INSTANTIATE_TEST_SUITE_P(
    AllPlatforms,
    StagerTest,
    testing::Values(Platform::LINUX_X64, Platform::LINUX_AARCH64, Platform::DARWIN_AARCH64),
    [](const testing::TestParamInfo<Platform>& info) -> std::string {
        switch (info.param) {
            case Platform::LINUX_X64:      return "LinuxX64";
            case Platform::LINUX_AARCH64:  return "LinuxAarch64";
            case Platform::DARWIN_AARCH64: return "DarwinAarch64";
        }
        return "Unknown";
    }
);
