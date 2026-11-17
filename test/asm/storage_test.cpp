// Storage tests. The execution tests publish words into an executable mapping
// and call them, so they run on an aarch64 host only.
#include <gtest/gtest.h>

#include <cstdint>

#include "asm/encoding.h"
#include "asm/operands.h"
#include "asm/storage.h"

namespace {

using Asm::ExecStorage;
using Asm::X0;
using Asm::X1;
using Asm::X30;

// Whether a storage reports no position and no written words: the state a
// storage is left in after being moved from.
bool IsEmpty(const ExecStorage& storage) { return storage.Position() == 0 && storage.Words().Size() == 0; }

#if defined(__aarch64__)
// The address of written code, called as a function of two integers.
using BinaryFunction = int64_t (*)(int64_t, int64_t);
#endif

} // namespace

TEST(ExecStorage, HoldsOneWordPerInstruction)
{
    ExecStorage storage { 64 };
    ASSERT_EQ(storage.Position(), 0u);

    storage.Write(Asm::Encode::Add(X0, X1, X1));
    storage.Write(Asm::Encode::Sub(X0, X0, X1));

    ASSERT_EQ(storage.Position(), 8u);
    ASSERT_EQ(storage.Words().Size(), 2u);
}

TEST(ExecStorage, PatchReplacesOneWordAndKeepsTheRest)
{
    ExecStorage storage { 64 };
    storage.Write(Asm::Encode::Add(X0, X1, X1));
    storage.Write(Asm::Encode::Sub(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));

    const auto before = storage.Words();
    ASSERT_EQ(before[1], Asm::Encode::Sub(X0, X0, X1).Bits());

    storage.Patch(4, Asm::Encode::Add(X0, X0, X1));

    const auto after = storage.Words();
    ASSERT_EQ(after[0], before[0]);
    ASSERT_EQ(after[1], Asm::Encode::Add(X0, X0, X1).Bits());
    ASSERT_EQ(after[2], before[2]);
    ASSERT_EQ(storage.Position(), 12u);
}

TEST(ExecStorage, MoveTransfersTheMapping)
{
    ExecStorage source { 64 };
    source.Write(Asm::Encode::Add(X0, X1, X1));

    ExecStorage moved { std::move(source) };

    // The mapping follows the move; the moved-from storage is left empty.
    ASSERT_TRUE(IsEmpty(source));
    ASSERT_EQ(moved.Position(), 4u);
    ASSERT_EQ(moved.Words().Size(), 1u);

    ExecStorage assigned { 64 };
    assigned.Write(Asm::Encode::Ret(X30));
    assigned = std::move(moved);

    ASSERT_TRUE(IsEmpty(moved));
    ASSERT_EQ(assigned.Position(), 4u);
    ASSERT_EQ(assigned.Words()[0], Asm::Encode::Add(X0, X1, X1).Bits());
}

TEST(ExecStorage, ExecutesWrittenWords)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    ExecStorage storage { 64 };
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));
    storage.Flush();

    auto add = reinterpret_cast<BinaryFunction>(storage.Code());
    ASSERT_EQ(add(40, 2), 42);
#endif
}

TEST(ExecStorage, PatchRewritesExecutedCode)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    ExecStorage storage { 64 };
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));
    storage.Flush();

    auto apply = reinterpret_cast<BinaryFunction>(storage.Code());
    ASSERT_EQ(apply(40, 2), 42);

    storage.Patch(0, Asm::Encode::Sub(X0, X0, X1));
    storage.Flush();
    ASSERT_EQ(apply(40, 2), 38);
#endif
}
