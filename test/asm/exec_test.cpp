// Assembler and label-manager tests. The execution tests emit code into an
// executable mapping and call it, so they run on an aarch64 host only.
#include <gtest/gtest.h>

#include <cstdint>

#include "asm/asm.h"
#include "asm/encoding.h"
#include "asm/labels.h"
#include "asm/operands.h"
#include "asm/storage.h"

namespace {

using Asm::Assembler;
using Asm::BasicLabelManager;
using Asm::ExecStorage;
using Asm::Label;
using Asm::X2;

#if defined(__aarch64__)
// Names used by the tests that call emitted code, which only compile where
// that code can run.
using Asm::W1;
using Asm::X0;
using Asm::X1;
using Asm::X3;
using Asm::X30;
using Asm::Xzr;

// The address of emitted code, called as a function of two integers.
using BinaryFunction = int64_t (*)(int64_t, int64_t);

// The same, for emitted code that takes a third integer.
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

TEST(AssemblerResolve, PatchesEachBranchWithItsOwnDistance)
{
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label loop = assembler.NewLabel();

    assembler.Bind(loop);
    assembler.Sub(X2, X2, 1);
    assembler.Cbnz(X2, loop);
    ASSERT_EQ(assembler.Words()[1], Asm::Encode::Cbnz(X2).Bits());

    assembler.Cbnz(X2, loop);
    assembler.Resolve();

    const auto words = assembler.Words();
    ASSERT_EQ(words[1], Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -4).Bits());
    ASSERT_EQ(words[2], Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -8).Bits());
    ASSERT_NE(words[1], words[2]);
}

TEST(ExecStorage, ExecutesEmittedArithmetic)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    assembler.Add(X0, X0, X1);
    assembler.Ret();
    assembler.Flush();

    auto add = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(add(20, 22), 42);
    ASSERT_EQ(add(-20, 5), -15);
#endif
}

TEST(ExecStorage, RunsACountingLoop)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label loop = assembler.NewLabel();

    assembler.Bind(loop);
    assembler.Add(X0, X0, X1);
    assembler.Sub(X2, X2, 1);
    assembler.Cbnz(X2, loop);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto count = reinterpret_cast<TernaryFunction>(assembler.Code());
    ASSERT_EQ(count(0, 3, 4), 12);
    ASSERT_EQ(count(0, -1, 5), -5);
    ASSERT_EQ(count(7, 0, 1), 7);
#endif
}

TEST(ExecStorage, BranchesAroundCodeConditionally)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label skip = assembler.NewLabel();

    assembler.Cbz(X1, skip);
    assembler.Add(X0, X0, X1);
    assembler.Bind(skip);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto addUnlessZero = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(addUnlessZero(2, 0), 2);
    ASSERT_EQ(addUnlessZero(2, 5), 7);
#endif
}

TEST(ExecStorage, CallsALabelAndReturns)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label function = assembler.NewLabel();

    assembler.Add(X2, X30, Xzr); // keep the caller's link register
    assembler.Bl(function);
    assembler.Add(X0, X0, X0);
    assembler.Add(X30, X2, Xzr); // put it back before returning to the caller
    assembler.Ret();
    assembler.Bind(function);
    assembler.Add(X0, X0, 1);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto incrementAndDouble = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(incrementAndDouble(5, 0), 12);
#endif
}

TEST(ExecStorage, BranchesThroughARegister)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label target = assembler.NewLabel();

    assembler.Adr(X1, target);
    assembler.Br(X1);
    assembler.Add(X0, X0, 100);
    assembler.Bind(target);
    assembler.Add(X0, X0, 41);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto addFortyOne = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(addFortyOne(1, 0), 42);
#endif
}

TEST(ExecStorage, BranchesThroughALinkRegister)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
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
    assembler.Resolve();
    assembler.Flush();

    auto callAndDouble = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(callAndDouble(1, 0), 12);
#endif
}

TEST(ExecStorage, BranchesWhenATestedBitIsClear)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label low = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Tbz(X1, 40, low);
    assembler.Add(X0, X0, 100);
    assembler.B(done);
    assembler.Bind(low);
    assembler.Add(X0, X0, 1);
    assembler.Bind(done);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto add = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(add(5, int64_t { 1 } << 40), 105);
    ASSERT_EQ(add(5, 0), 6);
#endif
}

TEST(ExecStorage, BranchesWhenATestedBitIsSet)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<ExecStorage> assembler { ExecStorage { 4096 } };
    const Label high = assembler.NewLabel();
    const Label done = assembler.NewLabel();

    assembler.Tbnz(W1, 4, high);
    assembler.Add(X0, X0, 1);
    assembler.B(done);
    assembler.Bind(high);
    assembler.Add(X0, X0, 100);
    assembler.Bind(done);
    assembler.Ret();
    assembler.Resolve();
    assembler.Flush();

    auto add = reinterpret_cast<BinaryFunction>(assembler.Code());
    ASSERT_EQ(add(5, int64_t { 1 } << 4), 105);
    ASSERT_EQ(add(5, 0), 6);
#endif
}
