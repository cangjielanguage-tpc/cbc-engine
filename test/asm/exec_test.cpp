// Assembler and label-manager tests. The execution tests publish the emitted
// words as an executable segment and call it, so they run on an aarch64 host
// only.
#include <gtest/gtest.h>

#include <cstdint>

#include "asm/asm.h"
#include "asm/code_storage.h"
#include "asm/encoding.h"
#include "asm/labels.h"
#include "asm/operands.h"

namespace {

using Asm::Assembler;
using Asm::BasicLabelManager;
using Asm::CodeSegment;
using Asm::CodeStorage;
using Asm::Label;
using Asm::X2;

#if defined(__aarch64__)
// Names used by the tests that call published code, which only compile where
// that code can run.
using Asm::W0;
using Asm::W1;
using Asm::W2;
using Asm::Wzr;
using Asm::X0;
using Asm::X1;
using Asm::X2;
using Asm::X3;
using Asm::X30;
using Asm::Xzr;

// The address of published code, called as a function of two integers.
using BinaryFunction = int64_t (*)(int64_t, int64_t);

// The same, for published code that takes a third integer.
using TernaryFunction = int64_t (*)(int64_t, int64_t, int64_t);
#endif

} // namespace

TEST(BasicLabelManager, BoundLabelsAnswerTheirPosition)
{
    BasicLabelManager labels;
    const Label loop = labels.NewLabel();
    const Label exit = labels.NewLabel();
    ASSERT_EQ(labels.LabelCount(), 2u);

    uint32_t position = 0;
    ASSERT_FALSE(labels.TryPosition(loop, position));

    labels.Bind(exit, 8);
    ASSERT_TRUE(labels.TryPosition(exit, position));
    ASSERT_EQ(position, 8u);

    labels.Bind(loop, 0);
    ASSERT_TRUE(labels.TryPosition(loop, position));
    ASSERT_EQ(position, 0u);
}

TEST(AssemblerFinalize, ResolvesBranchesBeforePublishing)
{
    Assembler<CodeStorage> assembler;
    const Label loop = assembler.NewLabel();

    assembler.Bind(loop);
    assembler.Sub(X2, X2, 1);
    assembler.Cbnz(X2, loop);
    // The branch word is emitted with an empty offset.
    ASSERT_EQ(assembler.Words()[1], Asm::Encode::Cbnz(X2).Bits());

    assembler.Cbnz(X2, loop);
    const CodeSegment segment = assembler.Finalize();

    ASSERT_TRUE(segment.Ok());
    ASSERT_EQ(segment.Words().Size(), 3u);
    ASSERT_EQ(
        segment.Words()[1], Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -4).Bits()
    );
    ASSERT_EQ(
        segment.Words()[2], Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -8).Bits()
    );
    ASSERT_NE(segment.Words()[1], segment.Words()[2]);
}

TEST(EmittedCode, ExecutesEmittedArithmetic)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    assembler.Add(X0, X0, X1);
    assembler.Ret();

    const CodeSegment segment = assembler.Finalize();
    auto add                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(add(20, 22), 42);
    ASSERT_EQ(add(-20, 5), -15);
#endif
}

TEST(EmittedCode, RunsACountingLoop)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label loop = assembler.NewLabel();

    assembler.Bind(loop);
    assembler.Add(X0, X0, X1);
    assembler.Sub(X2, X2, 1);
    assembler.Cbnz(X2, loop);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto count                = reinterpret_cast<TernaryFunction>(segment.Code());
    ASSERT_EQ(count(0, 3, 4), 12);
    ASSERT_EQ(count(0, -1, 5), -5);
    ASSERT_EQ(count(7, 0, 1), 7);
#endif
}

TEST(EmittedCode, BranchesAroundCodeConditionally)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label skip = assembler.NewLabel();

    assembler.Cbz(X1, skip);
    assembler.Add(X0, X0, X1);
    assembler.Bind(skip);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto addUnlessZero        = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(addUnlessZero(2, 0), 2);
    ASSERT_EQ(addUnlessZero(2, 5), 7);
#endif
}

TEST(EmittedCode, CallsALabelAndReturns)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label function = assembler.NewLabel();

    assembler.Add(X2, X30, Xzr); // keep the caller's link register
    assembler.Bl(function);
    assembler.Add(X0, X0, X0);
    assembler.Add(X30, X2, Xzr); // put it back before returning to the caller
    assembler.Ret();
    assembler.Bind(function);
    assembler.Add(X0, X0, 1);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto incrementAndDouble   = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(incrementAndDouble(5, 0), 12);
#endif
}

TEST(EmittedCode, BranchesThroughARegister)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label target = assembler.NewLabel();

    assembler.Adr(X1, target);
    assembler.Br(X1);
    assembler.Add(X0, X0, 100);
    assembler.Bind(target);
    assembler.Add(X0, X0, 41);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto addFortyOne          = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(addFortyOne(1, 0), 42);
#endif
}

TEST(EmittedCode, BranchesThroughALinkRegister)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label function = assembler.NewLabel();

    assembler.Add(X2, X30, Xzr); // keep the caller's link register
    assembler.Adr(X3, function);
    assembler.Blr(X3);
    assembler.Add(X0, X0, X0);
    assembler.Add(X30, X2, Xzr); // put it back before returning to the caller
    assembler.Ret();
    assembler.Bind(function);
    assembler.Add(X0, X0, 5);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto callAndDouble        = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(callAndDouble(1, 0), 12);
#endif
}

TEST(EmittedCode, BranchesWhenATestedBitIsClear)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label low  = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Tbz(X1, 40, low);
    assembler.Add(X0, X0, 100);
    assembler.B(done);
    assembler.Bind(low);
    assembler.Add(X0, X0, 1);
    assembler.Bind(done);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto add                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(add(5, int64_t { 1 } << 40), 105);
    ASSERT_EQ(add(5, 0), 6);
#endif
}

TEST(EmittedCode, BranchesWhenATestedBitIsSet)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label high = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Tbnz(W1, 4, high);
    assembler.Add(X0, X0, 1);
    assembler.B(done);
    assembler.Bind(high);
    assembler.Add(X0, X0, 100);
    assembler.Bind(done);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto add                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(add(5, int64_t { 1 } << 4), 105);
    ASSERT_EQ(add(5, 0), 6);
#endif
}

TEST(EmittedCode, CmpSetsFlagsForAConditionalBranch)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label ne   = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Cmp(X0, X1);
    assembler.B(Asm::Cc::Ne, ne);
    assembler.Add(X0, X0, X0); // equal: double the first operand
    assembler.B(done);
    assembler.Bind(ne);
    assembler.Sub(X0, X0, X0); // not equal: zero
    assembler.Bind(done);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto isEqual              = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(isEqual(21, 21), 42);
    ASSERT_EQ(isEqual(5, 6), 0);
    ASSERT_EQ(isEqual(-1, -1), -2);
#endif
}

TEST(EmittedCode, CmnSetsFlagsForAConditionalBranch)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label zero = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Cmn(X0, X1);
    assembler.B(Asm::Cc::Eq, zero);
    assembler.Sub(X0, X0, X0); // sum is not zero: zero
    assembler.B(done);
    assembler.Bind(zero);
    assembler.Add(X0, X0, 1); // sum is zero: the first operand, plus one
    assembler.Bind(done);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto isZeroSum            = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(isZeroSum(5, 5), 0);
    ASSERT_EQ(isZeroSum(5, -5), 6);
    ASSERT_EQ(isZeroSum(int64_t { 1 } << 62, -(int64_t { 1 } << 62)), (int64_t { 1 } << 62) + 1);
#endif
}

TEST(EmittedCode, LogicalImmediatesMaskAndCombine)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.And(X0, X0, 0xFF);                   // keep the low byte
    assembler.Orr(X0, X0, 0x100);                  // and set bit 8
    assembler.Eor(X0, X0, X1, Asm::Shift::Lsl, 8); // toggle bits taken from the second operand
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto combine              = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(combine(0x1234, 0), 0x134);
    ASSERT_EQ(combine(0x1234, 1), 0x34);
    ASSERT_EQ(combine(0xFF, 0), 0x1FF);
#endif
}

TEST(EmittedCode, TstSetsFlagsForAConditionalBranch)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;
    const Label negative = assembler.NewLabel();
    const Label done     = assembler.NewLabel();

    assembler.Tst(X0, 0x8000000000000000); // the sign bit: a rotated bitmask immediate
    assembler.B(Asm::Cc::Ne, negative);
    assembler.Add(X0, X0, 1);
    assembler.B(done);
    assembler.Bind(negative);
    assembler.Add(X0, X0, 100);
    assembler.Bind(done);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto add                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(add(-1, 0), 99);
    ASSERT_EQ(add(0, 0), 1);
    ASSERT_EQ(add(int64_t { 1 } << 63, 0), (int64_t { 1 } << 63) + 100);
#endif
}

TEST(EmittedCode, ExecutesVariableShifts)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Lsl(X0, X0, X1); // the first operand shifted left by the second
    assembler.Lsr(X2, X0, X1); // and back down, logically
    assembler.Add(X0, X0, X2);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto shift                = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(shift(1, 3), 8 + 1);
    ASSERT_EQ(shift(16, 4), 256 + 16);
    ASSERT_EQ(shift(7, 64), 7 + 7);  // an amount of the operand width shifts by zero
    ASSERT_EQ(shift(7, 65), 14 + 7); // and one more shifts by one
    ASSERT_EQ(shift(3, 63), static_cast<int64_t>(0x8000000000000000ull) + 1);
#endif
}

TEST(EmittedCode, ExecutesArithmeticShiftRight)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Asr(X1, X1, X0); // the second operand shifted right, replicating its sign
    assembler.Add(X0, X0, X1);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto shift                = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(shift(0, -1), -1);     // a shift of zero leaves a negative operand negative
    ASSERT_EQ(shift(3, -64), 3 - 8); // where a logical shift would have made it huge
    ASSERT_EQ(shift(2, 0x7FFFFFFFFFFFFFFF), 0x1FFFFFFFFFFFFFFF + 2);
    ASSERT_EQ(shift(64, 5), 64 + 5); // the shift amount is taken modulo 64: shift by zero
#endif
}

TEST(EmittedCode, ExecutesBitfieldExtractAndSignExtension)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Ubfx(X2, X1, 8, 8); // byte 1 of the second operand, zero-extended
    assembler.Lsr(X2, X2, 3);
    assembler.Sxtb(X0, W0); // the first operand's low byte, sign-extended to 64 bits
    assembler.Sxth(X1, W1); // the second operand's low halfword, likewise
    assembler.Add(X0, X0, X1);
    assembler.Add(X0, X0, X2);
    // SBFM with its fields spelled out: the low byte of the halfword again, but
    // sign-extended into 32 bits only, so the 64-bit value stays positive.
    assembler.Sbfm(W1, W1, 0, 7);
    assembler.Add(X0, X0, X1);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto extract              = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(extract(0x1234, 0x0B89), 0x34 + 0x0B89 + (0x0B >> 3) + int64_t { 0xFFFFFF89 });
    ASSERT_EQ(extract(0x80, 0x0B89), -0x80 + 0x0B89 + (0x0B >> 3) + int64_t { 0xFFFFFF89 });
    // Both sign extensions are negative here, so they cancel; the 32-bit one
    // does not, because a 32-bit result is written zero-extended.
    ASSERT_EQ(extract(-1, 0), -1);
    ASSERT_EQ(extract(-1, -1), -1 + -1 + (0xFF >> 3) + int64_t { 0xFFFFFFFF });
#endif
}

TEST(EmittedCode, ExecutesZeroExtensionAndWordSignExtension)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Uxtb(W1, W1);    // the low byte of the second operand, zero-extended to 32 bits
    assembler.Lsl(W1, W1, 24); // moved to the top of the word, where its sign bit is
    assembler.Sxtw(X1, W1);    // and read back as a signed 32-bit value, extended to 64 bits
    assembler.Asr(X1, X1, 2);  // an arithmetic shift, so a negative value stays negative
    assembler.Uxth(W0, W0);    // the low halfword of the first operand, zero-extended
    assembler.Lsl(X0, X0, 4);  // moved up out of the way
    assembler.Add(X0, X0, X1);
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto combine              = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(combine(0x1FFFF, 0x12), 0xFFFF0 + (int64_t { 0x12 } << 24) / 4);
    // 0x89000000 is negative as a 32-bit value: -1996488704, shifted right by two.
    ASSERT_EQ(combine(0x1FFFF, 0x89), 0xFFFF0 - 499122176);
    // 0xFF000000 is -16777216 as a 32-bit value, shifted right by two.
    ASSERT_EQ(combine(-1, 0xFF), 0xFFFF0 - 4194304);
#endif
}
