// Storage tests: accumulating words, patching them, and publishing them as an
// executable segment. The tests that call published code run on an aarch64 host
// only.
#include <gtest/gtest.h>

#include <cstdint>
#include <utility>

#include "asm/code_segment.h"
#include "asm/code_storage.h"
#include "asm/encoding.h"
#include "asm/operands.h"
#include "asm_test_support.h"

namespace {

using Asm::CodeSegment;
using Asm::CodeStorage;
using Asm::X0;
using Asm::X1;
using Asm::X2;
using Asm::X30;

#if defined(__aarch64__)
// The address of published code, called as a function of two integers.
using BinaryFunction = int64_t (*)(int64_t, int64_t);
#endif

} // namespace

TEST(CodeStorage, HoldsOneWordPerInstruction)
{
    CodeStorage storage;
    ASSERT_EQ(storage.Position(), 0u);

    storage.Write(Asm::Encode::Add(X0, X1, X1));
    storage.Write(Asm::Encode::Sub(X0, X0, X1));

    ASSERT_EQ(storage.Position(), 8u);
    ASSERT_EQ(WordCount(storage.Bytes()), 2u);
}

TEST(CodeStorage, PatchReplacesOneWordAndKeepsTheRest)
{
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X1, X1));
    storage.Write(Asm::Encode::Sub(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));

    const auto before = storage.Bytes();
    ASSERT_EQ(WordAt(before, 1), Asm::Encode::Sub(X0, X0, X1).Bits());

    storage.Patch(4, Asm::Encode::Add(X0, X0, X1));

    const auto after = storage.Bytes();
    ASSERT_EQ(WordAt(after, 0), WordAt(before, 0));
    ASSERT_EQ(WordAt(after, 1), Asm::Encode::Add(X0, X0, X1).Bits());
    ASSERT_EQ(WordAt(after, 2), WordAt(before, 2));
    ASSERT_EQ(storage.Position(), 12u);
}

TEST(CodeStorage, GrowthKeepsEarlierWordsPatchable)
{
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1)); // the word patched below
    for (uint32_t i = 0; i < 1000; ++i) {
        storage.Write(Asm::Encode::Add(X2, X2, 1));
    }

    storage.Patch(0, Asm::Encode::Sub(X0, X0, X1));

    ASSERT_EQ(storage.Position(), 1001u * 4u);
    ASSERT_EQ(WordAt(storage.Bytes(), 0), Asm::Encode::Sub(X0, X0, X1).Bits());
    ASSERT_EQ(WordAt(storage.Bytes(), 1000), Asm::Encode::Add(X2, X2, 1).Bits());
}

TEST(CodeStorage, MoveTransfersTheWords)
{
    CodeStorage source;
    source.Write(Asm::Encode::Add(X0, X1, X1));

    CodeStorage moved { std::move(source) };

    // The words follow the move; the moved-from storage is left empty.
    ASSERT_EQ(source.Position(), 0u);
    ASSERT_EQ(WordCount(source.Bytes()), 0u);
    ASSERT_EQ(moved.Position(), 4u);
    ASSERT_EQ(WordAt(moved.Bytes(), 0), Asm::Encode::Add(X0, X1, X1).Bits());
}

TEST(CodeStorage, FinalizePublishesTheWrittenWords)
{
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));

    const CodeSegment segment = storage.Finalize();

    ASSERT_TRUE(segment.Ok());
    ASSERT_NE(segment.Code(), nullptr);
    ASSERT_EQ(segment.Size(), 8u);
    ASSERT_EQ(WordCount(segment.Bytes()), 2u);
    ASSERT_EQ(WordAt(segment.Bytes(), 0), Asm::Encode::Add(X0, X0, X1).Bits());
    ASSERT_EQ(WordAt(segment.Bytes(), 1), Asm::Encode::Ret(X30).Bits());

    // The storage has published all it had: it holds no words anymore.
    ASSERT_EQ(storage.Position(), 0u);
    ASSERT_EQ(WordCount(storage.Bytes()), 0u);
}

TEST(CodeStorage, EmptyFinalizePublishesNothing)
{
    CodeStorage storage;

    const CodeSegment segment = storage.Finalize();

    ASSERT_FALSE(segment.Ok());
    ASSERT_EQ(segment.Size(), 0u);
}

TEST(CodeSegment, MoveTransfersTheMapping)
{
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));
    CodeSegment source = storage.Finalize();
    ASSERT_TRUE(source.Ok());

    CodeSegment moved { std::move(source) };

    ASSERT_FALSE(source.Ok());
    ASSERT_EQ(source.Size(), 0u);
    ASSERT_EQ(moved.Size(), 8u);
    ASSERT_EQ(WordAt(moved.Bytes(), 0), Asm::Encode::Add(X0, X0, X1).Bits());

    CodeSegment assigned;
    assigned = std::move(moved);

    ASSERT_FALSE(moved.Ok());
    ASSERT_EQ(WordCount(assigned.Bytes()), 2u);
}

TEST(CodeSegment, ExecutesPublishedWords)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));

    const CodeSegment segment = storage.Finalize();
    auto add                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(add(40, 2), 42);
#endif
}

TEST(CodeSegment, ExecutesWordsPatchedBeforePublication)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));
    storage.Patch(0, Asm::Encode::Sub(X0, X0, X1));

    const CodeSegment segment = storage.Finalize();
    auto sub                  = reinterpret_cast<BinaryFunction>(segment.Code());
    ASSERT_EQ(sub(40, 2), 38);
#endif
}

TEST(CodeSegment, OutlivesTheSegmentItWasMovedFrom)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    CodeStorage storage;
    storage.Write(Asm::Encode::Add(X0, X0, X1));
    storage.Write(Asm::Encode::Ret(X30));

    CodeSegment published = storage.Finalize();
    CodeSegment moved;
    {
        CodeSegment source = std::move(published);
        moved              = std::move(source);
    } // the intermediate owner releases nothing: the mapping followed the moves

    auto add = reinterpret_cast<BinaryFunction>(moved.Code());
    ASSERT_EQ(add(40, 2), 42);
#endif
}

TEST(CodeSegment, PublishedCodeIsNotWritable)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    CodeStorage storage;
    storage.Write(Asm::Encode::Ret(X30));

    const CodeSegment segment = storage.Finalize();
    ASSERT_DEATH(*static_cast<volatile uint32_t*>(segment.Code()) = 0, ".*");
#endif
}
