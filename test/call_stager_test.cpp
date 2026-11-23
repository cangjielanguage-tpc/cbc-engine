#include <gtest/gtest.h>

#include <optional>

#include "cbc/call_stager.h"
#include "cbc/frame.h"
#include "cbc/isa.h"
#include "regs_and_slots.h"
#include "utils/vector.h"

using namespace Cbc;

namespace {

constexpr uint32_t kFrameSize = 0x1000;

/// Everything that differs between the platforms under test, in one place so a
/// new platform touches a single switch.
struct PlatformInfo {
    AbiAssigner::PlatformDescription desc;
    CAbi abi;
    int intRegs;
    int fregs;
};

static PlatformInfo InfoFor(Platform p)
{
    switch (p) {
        case Platform::LINUX_X64:
            return { AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_X64>(),
                     CAbi::SYSV,
                     PlatformTraits<Platform::LINUX_X64>::IR_PARAM_COUNT,
                     PlatformTraits<Platform::LINUX_X64>::FR_PARAM_COUNT };
        case Platform::LINUX_AARCH64:
            return { AbiAssigner::PlatformDescription::FromTraits<Platform::LINUX_AARCH64>(),
                     CAbi::AAPCS64,
                     PlatformTraits<Platform::LINUX_AARCH64>::IR_PARAM_COUNT,
                     PlatformTraits<Platform::LINUX_AARCH64>::FR_PARAM_COUNT };
        case Platform::DARWIN_AARCH64:
            return { AbiAssigner::PlatformDescription::FromTraits<Platform::DARWIN_AARCH64>(),
                     CAbi::DARWIN_AARCH64,
                     PlatformTraits<Platform::DARWIN_AARCH64>::IR_PARAM_COUNT,
                     PlatformTraits<Platform::DARWIN_AARCH64>::FR_PARAM_COUNT };
    }
    FATAL("unknown platform");
}

struct StagerTest : public testing::TestWithParam<Platform> {
    FrameLayout layout { {}, {}, kFrameSize };
    RegsAndSlots sink;
    CallStager stager { sink, Info().desc, Info().abi };

    PlatformInfo Info() const { return InfoFor(GetParam()); }

    CAbi CAbiOf() const { return Info().abi; }

    int IntRegCount() const { return Info().intRegs; }

    int FloatRegCount() const { return Info().fregs; }

    Utils::Vector<int> argValues;

    void RecordArgs(Utils::Span<const Location> args)
    {
        argValues.Clear();
        for (auto arg : args) {
            argValues.PushBack(sink.Read(arg).value);
        }
    }

    int ValueAt(Location loc) const
    {
        if (loc.Kind() == Location::SLOT) {
            return sink.Slot(loc.SlotIdx()).value;
        }
        return sink.Read(loc).value;
    }
};

Location IRegLoc(int idx) { return Location::IReg(static_cast<IReg::Value>(idx)); }

Location FRegLoc(int idx) { return Location::FReg(static_cast<FReg::Value>(idx)); }

Location SlotLoc(int idx) { return Location::Slot(idx); }

/// Destination stack location for the n-th spilled argument.
Location StackArgLoc(int n) { return Location::Slot(n); }

CbcTypeKind IntParam() { return CbcTypeKind::I64; }

CbcTypeKind RefParam() { return CbcTypeKind::REF; }

CbcTypeKind RecParam() { return CbcTypeKind::REC; }

ForeignParam FlatRecParam(std::initializer_list<CAbiField> leaves, uint32_t size)
{
    ForeignParam p { CbcTypeKind::REC };
    for (auto leaf : leaves) {
        p.fields.PushBack(leaf);
    }
    p.flatSize = size;
    return p;
}

CbcTypeKind F64Param() { return CbcTypeKind::F64; }

CbcTypeKind F32Param() { return CbcTypeKind::F32; }

CallAbiFlags StaticFlags() { return { false, false, Image::MethodAbiKind::STATIC }; }

} // namespace

#define EXPECT_PLACED(i, where)                                                                                        \
    EXPECT_EQ(ValueAt(where), argValues[(i)]) << "argument " << (i) << " not placed in " << #where;

TEST_P(StagerTest, IntArgsLandInParameterRegisters)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
}

TEST_P(StagerTest, FloatArgsLandInFloatRegisters)
{
    Utils::Vector<CbcTypeKind> params { F64Param(), F32Param() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { FRegLoc(2), FRegLoc(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, FRegLoc(0));
    EXPECT_PLACED(1, FRegLoc(1));
}

TEST_P(StagerTest, MixedIntAndFloatArgsKeepIndependentCounters)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), F64Param(), IntParam(), F32Param() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(7), FRegLoc(2), IRegLoc(8), FRegLoc(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, FRegLoc(0));
    EXPECT_PLACED(2, IRegLoc(2));
    EXPECT_PLACED(3, FRegLoc(1));
}

TEST_P(StagerTest, IntArgsOverflowToSuccessiveStackSlots)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    int regs = IntRegCount();
    for (int i = 0; i < regs; i++) {
        EXPECT_EQ(ValueAt(IRegLoc(i + 1)), argValues[i]) << "argument " << i;
    }
    for (int i = regs; i < 9; i++) {
        EXPECT_EQ(ValueAt(StackArgLoc(i - regs)), argValues[i]) << "stack argument " << i;
    }
}

TEST_P(StagerTest, FloatArgsOverflowToStackSlots)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < 9; i++) {
        params.PushBack(F64Param());
        args.PushBack(FRegLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    int regs = FloatRegCount();
    for (int i = 0; i < regs; i++) {
        EXPECT_EQ(ValueAt(FRegLoc(i)), argValues[i]) << "float argument " << i;
    }
    EXPECT_EQ(ValueAt(StackArgLoc(0)), argValues[regs]) << "spilled float argument";
}

TEST_P(StagerTest, ArgumentFromUntypedSlotLandsInRegister)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(2) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
}

TEST_P(StagerTest, SwappedArgumentsAreNotClobbered)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(2), IRegLoc(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
}

TEST_P(StagerTest, RotatedArgumentsAreNotClobbered)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), IntParam(), IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(2), IRegLoc(3), IRegLoc(1) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
    EXPECT_PLACED(2, IRegLoc(3));
}

TEST_P(StagerTest, RefArgOnStackIsReportedAsParamRefSlot)
{
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i <= IntRegCount(); i++) {
        params.PushBack(RefParam());
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
        params.PushBack(RecParam());
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
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6), IRegLoc(7) };

    RecordArgs(args);
    CallAbiFlags flags { false, false, Image::MethodAbiKind::MUT };
    auto result = stager.StageManaged(args, flags, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(ValueAt(IRegLoc(1)), argValues[0]) << "derived receiver";
    EXPECT_EQ(ValueAt(IRegLoc(2)), argValues[1]) << "base receiver";
    EXPECT_EQ(ValueAt(IRegLoc(3)), argValues[2]) << "first parameter";
    ASSERT_TRUE(result.derived.has_value());
    EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR1));
}

TEST_P(StagerTest, GenericClosureParametersArePassedAsReferences)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    CallAbiFlags flags { false, true, Image::MethodAbiKind::REF_RECEIVER };
    auto result = stager.StageManaged(args, flags, sig, nullptr, nullptr, /*genericParams=*/true);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
    EXPECT_PLACED(2, IRegLoc(3));
    EXPECT_PLACED(3, IRegLoc(4));
}

TEST_P(StagerTest, OuterTiAndThisTiLocationsAreReported)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5) };

    Location outerTiLoc = Location::Nil();
    Location thisTiLoc  = Location::Nil();
    CallAbiFlags flags { false, true, Image::MethodAbiKind::HAS_THIS_TI };
    auto result = stager.StageManaged(args, flags, sig, &outerTiLoc, &thisTiLoc);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(outerTiLoc.Kind(), Location::IREG);
    EXPECT_EQ(thisTiLoc.Kind(), Location::IREG);
    EXPECT_NE(outerTiLoc.IRegIdx(), thisTiLoc.IRegIdx());
}

TEST_P(StagerTest, ManagedArgCountMismatchFails)
{
    Utils::Vector<CbcTypeKind> params { IntParam(), IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(3) };

    auto result = stager.StageManaged(args, StaticFlags(), sig);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.failure.empty());
}

TEST_P(StagerTest, ForeignIntArgsLandInParameterRegisters)
{
    Utils::Vector<ForeignParam> params { IntParam(), IntParam() };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
}

TEST_P(StagerTest, ForeignFloatArgsLandInFloatRegisters)
{
    Utils::Vector<ForeignParam> params { F64Param() };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { FRegLoc(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, FRegLoc(0));
}

TEST_P(StagerTest, ForeignIntArgsOverflowToStack)
{
    int regs = IntRegCount();
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs + 2; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }

    ForeignCallSignature sig { params };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    for (int i = 0; i < regs; i++) {
        EXPECT_EQ(ValueAt(IRegLoc(i + 1)), argValues[i]) << "argument " << i;
    }
    EXPECT_EQ(ValueAt(StackArgLoc(0)), argValues[regs]) << "first spilled argument";
    EXPECT_EQ(ValueAt(StackArgLoc(1)), argValues[regs + 1]) << "second spilled argument";
    EXPECT_EQ(staging.stackBytes, 2 * Cbc::STACK_SLOT_SIZE);
}

TEST_P(StagerTest, ForeignStackArgFromUntypedSlotLandsInStackSlot)
{
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    int spillIdx = IntRegCount();
    for (int i = 0; i <= spillIdx; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    args[spillIdx] = SlotLoc(2);

    ForeignCallSignature sig { params };
    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_EQ(ValueAt(StackArgLoc(0)), argValues[spillIdx]);
    EXPECT_EQ(staging.stackBytes, Cbc::STACK_SLOT_SIZE);
}

TEST_P(StagerTest, ForeignVarArgLabelIsNotAnArgument)
{
    ForeignParam label;
    label.isVarArgLabel = true;

    Utils::Vector<ForeignParam> params { IntParam(), label, IntParam() };
    ForeignCallSignature sig { params, /*isVarArg=*/true };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4) };

    RecordArgs(args);
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    switch (CAbiOf()) {
        case CAbi::SYSV:
        case CAbi::AAPCS64:        EXPECT_PLACED(1, IRegLoc(2)); break;
        case CAbi::DARWIN_AARCH64: EXPECT_EQ(ValueAt(StackArgLoc(0)), argValues[1]); break;
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
    EXPECT_PLACED(0, IRegLoc(1));
}

TEST_P(StagerTest, ForeignArgCountMismatchFails)
{
    Utils::Vector<ForeignParam> params { IntParam() };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { IRegLoc(1), IRegLoc(2) };

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    EXPECT_FALSE(staging.result.ok);
    EXPECT_FALSE(staging.result.failure.empty());
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
    // Each piece is loaded from the caller copy at its own byte offset.
    EXPECT_EQ(sink.iregs[1].tag, Cell::LOAD);
    EXPECT_EQ(sink.iregs[1].offset, 0);
    EXPECT_EQ(sink.iregs[1].value, argValues[0]);
    EXPECT_EQ(sink.iregs[2].tag, Cell::LOAD);
    EXPECT_EQ(sink.iregs[2].offset, 8);
}

TEST_P(StagerTest, ForeignLargeStructAbove16Bytes)
{
    Utils::Vector<ForeignParam> params {
        FlatRecParam({ { 0, 8, false }, { 8, 8, false }, { 16, 8, false }, { 24, 8, false } }, 32)
    };
    ForeignCallSignature sig { params };
    Utils::Vector<Location> args { SlotLoc(0) };

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    switch (CAbiOf()) {
        case CAbi::SYSV:
            // Copied onto the stack, one slot per 8 bytes.
            EXPECT_EQ(staging.stackBytes, 4 * Cbc::STACK_SLOT_SIZE);
            for (uint32_t i = 0; i < 4; i++) {
                EXPECT_EQ(sink.Slot(i).tag, Cell::LOAD);
                EXPECT_EQ(sink.Slot(i).offset, static_cast<int32_t>(i * 8));
            }
            break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64:
            // Passed as a pointer to a caller-made copy in IR1.
            EXPECT_EQ(staging.stackBytes, 0);
            EXPECT_NE(sink.iregs[1].tag, Cell::NONE);
            break;
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

    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    switch (CAbiOf()) {
        case CAbi::SYSV:
            // Each 8-byte chunk is one piece: two F64 registers.
            EXPECT_EQ(staging.fregs, 2);
            EXPECT_EQ(sink.fregs[0].tag, Cell::LOAD);
            EXPECT_EQ(sink.fregs[1].offset, 8);
            break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64:
            // HFA: one FP register per element.
            EXPECT_EQ(staging.fregs, 4);
            for (uint32_t i = 0; i < 4; i++) {
                EXPECT_EQ(sink.fregs[i].tag, Cell::LOAD);
                EXPECT_EQ(sink.fregs[i].offset, static_cast<int32_t>(i * 4));
            }
            break;
    }
}

TEST_P(StagerTest, ForeignStructRetPlanUsesPrecomputedLayout)
{
    // 16-byte record is returned in registers, so no sret pointer is consumed.
    ForeignCallSignature sig;
    sig.retFields.PushBack({ 0, 8, false });
    sig.retFields.PushBack({ 8, 8, false });
    sig.retSize = 16;

    auto plan = stager.StageForeignStructRet(sig, { true, false, Image::MethodAbiKind::STATIC });

    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->passBy, CAbiPassBy::REGS);
    EXPECT_EQ(plan->PiecesNum(), 2);
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
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(4), IRegLoc(3) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { true, false, Image::MethodAbiKind::STATIC }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
            // sret takes IR1 as a record, so the parameter is shifted to IR2.
            EXPECT_EQ(ValueAt(IRegLoc(1)), argValues[0]) << "sret destination";
            EXPECT_EQ(ValueAt(IRegLoc(2)), argValues[1]) << "shifted parameter";
            break;
        case Platform::LINUX_AARCH64:
        case Platform::DARWIN_AARCH64:
            // sret goes to a dedicated register, parameters start at IR1.
            EXPECT_EQ(ValueAt(IRegLoc(1)), argValues[1]) << "parameter is not shifted";
            EXPECT_EQ(ValueAt(IRegLoc(9)), argValues[0]) << "dedicated sret register";
            break;
    }
}

TEST_P(StagerTest, ForeignStructRetUsesPlatformFpBudget)
{
    // All-float 16-byte struct: 2 pieces on SysV, 4 on AAPCS64/Darwin.
    Utils::Vector<CAbiField> fields { { 0, 4, true }, { 4, 4, true }, { 8, 4, true }, { 12, 4, true } };

    auto plan = ClassifyCAbiStructRet(CAbiOf(), fields, 16);

    ASSERT_EQ(plan.passBy, CAbiPassBy::REGS);
    switch (CAbiOf()) {
        case CAbi::SYSV:           EXPECT_EQ(plan.PiecesNum(), 2); break;
        case CAbi::AAPCS64:
        case CAbi::DARWIN_AARCH64: EXPECT_EQ(plan.PiecesNum(), 4); break;
    }
}

TEST_P(StagerTest, PrimReceiverTakesIntegerRegister)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::PRIM_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
}

TEST_P(StagerTest, FPrimReceiverTakesFloatRegister)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { FRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::FPRIM_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, FRegLoc(0));
    EXPECT_PLACED(1, IRegLoc(1));
}

TEST_P(StagerTest, RecReceiverTakesIntegerRegisterAndDoesNotShiftSret)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { false, false, Image::MethodAbiKind::REC_RECEIVER }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
}

TEST_P(StagerTest, FuncTypeVariablesTakeIntegerRegisters)
{
    // Generic instance: type vars are passed as trailing integer arguments.
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, /*funcVarCount=*/2 };
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_PLACED(0, IRegLoc(1));
    EXPECT_PLACED(1, IRegLoc(2));
    EXPECT_PLACED(2, IRegLoc(3));
}

TEST_P(StagerTest, FuncTypeVariablesOverflowToStackAfterParams)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // Two type vars cannot fit, they spill in order after the parameters.
    args.PushBack(IRegLoc(regs + 1));
    args.PushBack(IRegLoc(regs + 2));

    ManagedCallSignature sig { params, 2 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(ValueAt(StackArgLoc(0)), argValues[regs]);
    EXPECT_EQ(ValueAt(StackArgLoc(1)), argValues[regs + 1]);
}

TEST_P(StagerTest, GenericParamsIgnoreDeclaredKindsAndPassReferences)
{
    // Generic closure: every parameter is passed as a managed reference,
    // regardless of the declared kind.
    CbcTypeKind rec = RecParam();
    CbcTypeKind f64 = F64Param();
    Utils::Vector<CbcTypeKind> params { rec, f64 };
    ManagedCallSignature sig { params, 0 };
    // Receiver, two parameters, outer type info.
    Utils::Vector<Location> args { IRegLoc(3), IRegLoc(4), IRegLoc(5), IRegLoc(6) };

    RecordArgs(args);
    auto result = stager.StageManaged(
        args, { false, true, Image::MethodAbiKind::REF_RECEIVER }, sig, nullptr, nullptr, /*genericParams=*/true
    );

    ASSERT_TRUE(result.ok) << result.failure;
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(ValueAt(IRegLoc(i + 1)), argValues[i]) << "argument " << i;
    }
}

TEST_P(StagerTest, MutReceiverWithSRet)
{
    Utils::Vector<CbcTypeKind> params { IntParam() };
    ManagedCallSignature sig { params, 0 };
    Utils::Vector<Location> args { SlotLoc(4), IRegLoc(5), IRegLoc(6), IRegLoc(7) };

    RecordArgs(args);
    auto result = stager.StageManaged(args, { true, false, Image::MethodAbiKind::MUT }, sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
            // sret shifts everything: sret, derived, base, param.
            EXPECT_EQ(ValueAt(IRegLoc(1)), argValues[0]);
            EXPECT_EQ(ValueAt(IRegLoc(2)), argValues[1]);
            EXPECT_EQ(ValueAt(IRegLoc(3)), argValues[2]);
            EXPECT_EQ(ValueAt(IRegLoc(4)), argValues[3]);
            EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR2));
            break;
        case Platform::LINUX_AARCH64:
        case Platform::DARWIN_AARCH64:
            // Dedicated sret register: derived, base, param start at IR1.
            EXPECT_EQ(ValueAt(IRegLoc(9)), argValues[0]);
            EXPECT_EQ(ValueAt(IRegLoc(1)), argValues[1]);
            EXPECT_EQ(ValueAt(IRegLoc(2)), argValues[2]);
            EXPECT_EQ(ValueAt(IRegLoc(3)), argValues[3]);
            EXPECT_EQ(result.derived.value(), static_cast<int>(IReg::IR1));
            break;
    }
}

// ---------------------------------------------------------------------------
// Darwin compact stack slots: expected to fail until the engine packs named
// stack arguments by natural size instead of one 8-byte slot each.
// See docs/darwin-abi-plan.md, phases 1-3.
// ---------------------------------------------------------------------------

TEST_P(StagerTest, CompactSlotsFitSmallArgs)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // Four i16 arguments fill a single 8-byte stack region on Darwin.
    for (int i = 0; i < 4; i++) {
        params.PushBack(CbcTypeKind::I16);
        args.PushBack(SlotLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64:  EXPECT_EQ(result.maxParamPassingSize, 4 * Cbc::STACK_SLOT_SIZE); break;
        case Platform::DARWIN_AARCH64: EXPECT_EQ(result.maxParamPassingSize, 8u); break;
    }
}

TEST_P(StagerTest, CompactSlotsPackMixedWidths)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // i32 + i16 + i16: 4 + 2 + 2 = 8 bytes on Darwin, 24 on Linux.
    params.PushBack(CbcTypeKind::I32);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I16);
    args.PushBack(SlotLoc(0));
    args.PushBack(SlotLoc(1));
    args.PushBack(SlotLoc(2));

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64:  EXPECT_EQ(result.maxParamPassingSize, 3 * Cbc::STACK_SLOT_SIZE); break;
        case Platform::DARWIN_AARCH64: EXPECT_EQ(result.maxParamPassingSize, 8u); break;
    }
}

TEST_P(StagerTest, CompactSlotsOnlySingleByteArgs)
{
    int regs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < regs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // Eight booleans occupy exactly one 8-byte region on Darwin.
    for (int i = 0; i < 8; i++) {
        params.PushBack(CbcTypeKind::BOOL);
        args.PushBack(SlotLoc(i));
    }

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64:  EXPECT_EQ(result.maxParamPassingSize, 8 * Cbc::STACK_SLOT_SIZE); break;
        case Platform::DARWIN_AARCH64: EXPECT_EQ(result.maxParamPassingSize, 8u); break;
    }
}

TEST_P(StagerTest, CompactSlotsForForeignNamedArgs)
{
    // Named (non-variadic) foreign args pack on Darwin; varargs stay 8-byte.
    int nRegs = IntRegCount();
    Utils::Vector<ForeignParam> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < nRegs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    for (int i = 0; i < 4; i++) {
        params.PushBack(CbcTypeKind::I16);
        args.PushBack(SlotLoc(i));
    }

    ForeignCallSignature sig { params };
    auto staging = stager.StageForeign(args, StaticFlags(), sig, std::nullopt);

    ASSERT_TRUE(staging.result.ok) << staging.result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64:
            EXPECT_EQ(staging.stackBytes, 4 * Cbc::STACK_SLOT_SIZE);
            for (int i = 0; i < 4; i++) {
                EXPECT_EQ(sink.ParamAt(i, 0).tag, Cell::MOVE);
            }
            break;
        case Platform::DARWIN_AARCH64:
            EXPECT_EQ(staging.stackBytes, Cbc::STACK_SLOT_SIZE);
            // All four share slot 0 at consecutive 2-byte offsets.
            for (int i = 0; i < 4; i++) {
                EXPECT_EQ(sink.ParamAt(0, i * 2).tag, Cell::MOVE) << "i16 at byte " << i * 2;
                EXPECT_EQ(static_cast<int>(sink.SlotSizes[i * 2]), static_cast<int>(Format::StoreAccessKind::ST_16));
            }
            break;
    }
}

TEST_P(StagerTest, CompactSlotsRespectNaturalAlignment)
{
    int nRegs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < nRegs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // A 1-byte argument followed by a 64-bit one: the latter must be aligned,
    // not packed at offset 1.
    params.PushBack(CbcTypeKind::BOOL);
    params.PushBack(IntParam());
    args.PushBack(SlotLoc(0));
    args.PushBack(SlotLoc(1));

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    EXPECT_EQ(result.maxParamPassingSize, 2 * Cbc::STACK_SLOT_SIZE);
    // The 64-bit argument occupies its own slot, aligned within the area.
    EXPECT_EQ(sink.ParamAt(1, 0).tag, Cell::MOVE);
}

TEST_P(StagerTest, CompactedArgumentFromFrameSlotKeepsItsWidth)
{
    int nRegs = IntRegCount();
    Utils::Vector<CbcTypeKind> params;
    Utils::Vector<Location> args;
    for (int i = 0; i < nRegs; i++) {
        params.PushBack(IntParam());
        args.PushBack(IRegLoc(i + 1));
    }
    // The spilled arguments arrive from untyped frame slots, taking the
    // frame-slot -> passing-area path rather than a register move.
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I16);
    args.PushBack(SlotLoc(0));
    args.PushBack(SlotLoc(1));

    ManagedCallSignature sig { params, 0 };
    auto result = stager.StageManaged(args, StaticFlags(), sig);

    ASSERT_TRUE(result.ok) << result.failure;
    switch (GetParam()) {
        case Platform::LINUX_X64:
        case Platform::LINUX_AARCH64: EXPECT_EQ(result.maxParamPassingSize, 2 * Cbc::STACK_SLOT_SIZE); break;
        case Platform::DARWIN_AARCH64:
            // Both i16s share one slot and must be stored with their own width.
            EXPECT_EQ(result.maxParamPassingSize, Cbc::STACK_SLOT_SIZE);
            EXPECT_EQ(static_cast<int>(sink.SlotSizes[0]), static_cast<int>(Format::StoreAccessKind::ST_16));
            break;
    }
}

// ---------------------------------------------------------------------------
// Tail parameters: the callee reads stack arguments via `ld.tail N`, where N is
// the ordinal of the n-th stack-passed parameter. The rewriter maps each
// ordinal to a byte offset with ComputeTailParams, which must agree with the
// placement StageManaged produces for the same signature.
// ---------------------------------------------------------------------------

TEST_P(StagerTest, TailOffsetsPackPerPlatform)
{
    Utils::Vector<CbcTypeKind> params;
    for (int i = 0; i < IntRegCount(); i++) {
        params.PushBack(IntParam());
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(F64Param());
    }
    // Every following parameter overflows to the passing area.
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(IntParam());
    params.PushBack(F32Param());
    params.PushBack(CbcTypeKind::I16);

    ManagedCallSignature sig { params, 0 };
    auto tail = ComputeTailParams(sig, StaticFlags(), Info().desc);

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
        params.PushBack(IntParam());
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(F64Param());
    }
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I8);
    params.PushBack(CbcTypeKind::U16);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(CbcTypeKind::I32);
    params.PushBack(F32Param());
    params.PushBack(IntParam());
    params.PushBack(RefParam());
    params.PushBack(RecParam());

    ManagedCallSignature sig { params, 0 };
    auto tail = ComputeTailParams(sig, StaticFlags(), Info().desc);

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
        params.PushBack(IntParam());
    }
    for (int i = 0; i < FloatRegCount(); i++) {
        params.PushBack(F64Param());
    }
    params.PushBack(CbcTypeKind::U8);
    params.PushBack(CbcTypeKind::I16);
    params.PushBack(IntParam());
    params.PushBack(F32Param());
    params.PushBack(RefParam());
    params.PushBack(RecParam());
    for (size_t i = 0; i < params.Size(); i++) {
        args.PushBack(SlotLoc(static_cast<int>(i)));
    }

    ManagedCallSignature sig { params, 0 };
    RecordArgs(args);
    auto result = stager.StageManaged(args, StaticFlags(), sig);
    ASSERT_TRUE(result.ok) << result.failure;

    auto tail = ComputeTailParams(sig, StaticFlags(), Info().desc);

    // Each argument that landed in the passing area must take the next tail
    // ordinal, at exactly the byte offset the caller-side staging chose.
    int ordinal = 0;
    for (size_t i = 0; i < params.Size(); i++) {
        std::optional<size_t> at;
        for (size_t off = 0; off < sink.stackSlotCount * 8; off++) {
            if (sink.paramBytes[off].tag == Cell::MOVE && sink.paramBytes[off].value == argValues[i]) {
                at = off;
                break;
            }
        }
        if (!at.has_value()) {
            continue; // argument landed in a register
        }
        ASSERT_LT(ordinal, static_cast<int>(tail.Size())) << "argument " << i << " has no tail ordinal";
        EXPECT_EQ(tail[ordinal].offset, static_cast<uint32_t>(*at))
            << "tail ordinal " << ordinal << " for argument " << i;
        EXPECT_EQ(static_cast<int>(sink.SlotSizes[*at]), static_cast<int>(StkArg(params[i])))
            << "store width for argument " << i;
        ordinal++;
    }
    EXPECT_EQ(ordinal, static_cast<int>(tail.Size()));
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
