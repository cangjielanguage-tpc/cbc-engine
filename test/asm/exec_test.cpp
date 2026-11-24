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
#include "asm_test_support.h"

namespace {

// Shorthand for the encoder that each method forwards to.
namespace Encode = Asm::Encode;

using Asm::Assembler;
using Asm::BasicLabelManager;
using Asm::CodeSegment;
using Asm::CodeStorage;
using Asm::IndexExtend;
using Asm::Insn;
using Asm::Label;
using Asm::Lsl12;
using Asm::Mem;
using Asm::Scaled;
using Asm::Shift;
using Asm::Sp;
using Asm::W0;
using Asm::W1;
using Asm::W2;
using Asm::W3;
using Asm::W4;
using Asm::Wzr;
using Asm::X0;
using Asm::X1;
using Asm::X2;
using Asm::X3;
using Asm::X30;
using Asm::X4;
using Asm::Xzr;

#if defined(__aarch64__)
// The address of published code, called as a function of two integers.
using BinaryFunction = int64_t (*)(int64_t, int64_t);

// The same, for published code that takes a third integer.
using TernaryFunction = int64_t (*)(int64_t, int64_t, int64_t);

// Published code that takes no argument and returns the register it moved into.
using NullaryFunction = uint64_t (*)();
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
    ASSERT_EQ(WordAt(assembler.Bytes(), 1), Asm::Encode::Cbnz(X2).Bits());

    assembler.Cbnz(X2, loop);
    const CodeSegment segment = assembler.Finalize();

    ASSERT_TRUE(segment.Ok());
    ASSERT_EQ(WordCount(segment.Bytes()), 3u);
    ASSERT_EQ(
        WordAt(segment.Bytes(), 1),
        Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -4).Bits()
    );
    ASSERT_EQ(
        WordAt(segment.Bytes(), 2),
        Asm::Encode::ApplyFixup(Asm::Encode::Cbnz(X2), Asm::FixupKind::CmpBranch19, -8).Bits()
    );
    ASSERT_NE(WordAt(segment.Bytes(), 1), WordAt(segment.Bytes(), 2));
}

// The assembler methods are the API the callers use; each must emit exactly the
// word of the encoder it forwards to. The zero register and SP are separate
// operand types, so which overload a call resolves to is worth pinning down
// here, next to the entry points, rather than only through the encoders.
TEST(AssemblerMethods, EmitTheEncodersWords)
{
    struct Sample {
        const char* text;
        Insn encoded;
        void (*emit)(Assembler<CodeStorage>&);
    };

    const Sample samples[] = {
        { "add x0, x1, #9", Encode::Add(X0, X1, 9), [](Assembler<CodeStorage>& a) { a.Add(X0, X1, 9); } },
        { "add x0, sp, #9", Encode::Add(X0, Sp, 9), [](Assembler<CodeStorage>& a) { a.Add(X0, Sp, 9); } },
        { "add sp, x1, #9", Encode::Add(Sp, X1, 9), [](Assembler<CodeStorage>& a) { a.Add(Sp, X1, 9); } },
        { "add sp, sp, #9", Encode::Add(Sp, Sp, 9), [](Assembler<CodeStorage>& a) { a.Add(Sp, Sp, 9); } },
        { "add w0, w1, #9, lsl #12",
          Encode::Add(W0, W1, 9, Lsl12),
          [](Assembler<CodeStorage>& a) { a.Add(W0, W1, 9, Lsl12); } },
        { "add x0, x1, x2, asr #8",
          Encode::Add(X0, X1, X2, Shift::Asr, 8),
          [](Assembler<CodeStorage>& a) { a.Add(X0, X1, X2, Shift::Asr, 8); } },
        { "add x0, xzr, x2", Encode::Add(X0, Xzr, X2), [](Assembler<CodeStorage>& a) { a.Add(X0, Xzr, X2); } },
        { "add x0, x1, xzr", Encode::Add(X0, X1, Xzr), [](Assembler<CodeStorage>& a) { a.Add(X0, X1, Xzr); } },
        { "subs xzr, x1, #9", Encode::Subs(Xzr, X1, 9), [](Assembler<CodeStorage>& a) { a.Subs(Xzr, X1, 9); } },
        { "subs xzr, sp, #9", Encode::Subs(Xzr, Sp, 9), [](Assembler<CodeStorage>& a) { a.Subs(Xzr, Sp, 9); } },
        { "adds xzr, sp, #9, lsl #12",
          Encode::Adds(Xzr, Sp, 9, Lsl12),
          [](Assembler<CodeStorage>& a) { a.Adds(Xzr, Sp, 9, Lsl12); } },
        { "adds x0, xzr, xzr", Encode::Adds(X0, Xzr, Xzr), [](Assembler<CodeStorage>& a) { a.Adds(X0, Xzr, Xzr); } },
        { "subs wzr, w1, #9, lsl #12",
          Encode::Subs(Wzr, W1, 9, Lsl12),
          [](Assembler<CodeStorage>& a) { a.Subs(Wzr, W1, 9, Lsl12); } },
        { "cmp x1, x2, lsr #4",
          Encode::Subs(Xzr, X1, X2, Shift::Lsr, 4),
          [](Assembler<CodeStorage>& a) { a.Cmp(X1, X2, Shift::Lsr, 4); } },
        { "cmp sp, #9", Encode::Subs(Xzr, Sp, 9), [](Assembler<CodeStorage>& a) { a.Cmp(Sp, 9); } },
        { "cmn w1, #9", Encode::Adds(Wzr, W1, 9), [](Assembler<CodeStorage>& a) { a.Cmn(W1, 9); } },
        { "and x0, x1, #0xff", Encode::And(X0, X1, 0xFF), [](Assembler<CodeStorage>& a) { a.And(X0, X1, 0xFF); } },
        { "orr sp, x1, #0x100", Encode::Orr(Sp, X1, 0x100), [](Assembler<CodeStorage>& a) { a.Orr(Sp, X1, 0x100); } },
        { "eor w0, w1, #0xff", Encode::Eor(W0, W1, 0xFF), [](Assembler<CodeStorage>& a) { a.Eor(W0, W1, 0xFF); } },
        { "ands xzr, x1, #0xff",
          Encode::Ands(Xzr, X1, 0xFF),
          [](Assembler<CodeStorage>& a) { a.Ands(Xzr, X1, 0xFF); } },
        { "tst x1, #0xff", Encode::Ands(Xzr, X1, 0xFF), [](Assembler<CodeStorage>& a) { a.Tst(X1, 0xFF); } },
        { "tst w1, #0xff", Encode::Ands(Wzr, W1, 0xFF), [](Assembler<CodeStorage>& a) { a.Tst(W1, 0xFF); } },
        { "and xzr, x1, x2, lsl #3",
          Encode::And(Xzr, X1, X2, Shift::Lsl, 3),
          [](Assembler<CodeStorage>& a) { a.And(Xzr, X1, X2, Shift::Lsl, 3); } },
        { "lsl x0, x1, x2", Encode::Lsl(X0, X1, X2), [](Assembler<CodeStorage>& a) { a.Lsl(X0, X1, X2); } },
        { "lsr w0, wzr, #4", Encode::Lsr(W0, Wzr, 4), [](Assembler<CodeStorage>& a) { a.Lsr(W0, Wzr, 4); } },
        { "ubfm xzr, x1, #0, #7",
          Encode::Ubfm(Xzr, X1, 0, 7),
          [](Assembler<CodeStorage>& a) { a.Ubfm(Xzr, X1, 0, 7); } },
        { "sbfx w0, w1, #4, #4", Encode::Sbfx(W0, W1, 4, 4), [](Assembler<CodeStorage>& a) { a.Sbfx(W0, W1, 4, 4); } },
        { "sxtb x0, w1", Encode::Sxtb(X0, W1), [](Assembler<CodeStorage>& a) { a.Sxtb(X0, W1); } },
        { "uxtb wzr, w1", Encode::Uxtb(Wzr, W1), [](Assembler<CodeStorage>& a) { a.Uxtb(Wzr, W1); } },
        { "mul x0, x1, x2", Encode::Mul(X0, X1, X2), [](Assembler<CodeStorage>& a) { a.Mul(X0, X1, X2); } },
        { "mul wzr, w1, w2", Encode::Mul(Wzr, W1, W2), [](Assembler<CodeStorage>& a) { a.Mul(Wzr, W1, W2); } },
        { "madd x0, x1, x2, x3",
          Encode::Madd(X0, X1, X2, X3),
          [](Assembler<CodeStorage>& a) { a.Madd(X0, X1, X2, X3); } },
        { "madd xzr, x1, x2, xzr",
          Encode::Madd(Xzr, X1, X2, Xzr),
          [](Assembler<CodeStorage>& a) { a.Madd(Xzr, X1, X2, Xzr); } },
        { "msub w0, w1, w2, w3",
          Encode::Msub(W0, W1, W2, W3),
          [](Assembler<CodeStorage>& a) { a.Msub(W0, W1, W2, W3); } },
        { "msub x0, x1, x2, xzr",
          Encode::Msub(X0, X1, X2, Xzr),
          [](Assembler<CodeStorage>& a) { a.Msub(X0, X1, X2, Xzr); } },
        { "sdiv x0, x1, x2", Encode::Sdiv(X0, X1, X2), [](Assembler<CodeStorage>& a) { a.Sdiv(X0, X1, X2); } },
        { "sdiv xzr, xzr, xzr", Encode::Sdiv(Xzr, Xzr, Xzr), [](Assembler<CodeStorage>& a) { a.Sdiv(Xzr, Xzr, Xzr); } },
        { "udiv wzr, w1, w2", Encode::Udiv(Wzr, W1, W2), [](Assembler<CodeStorage>& a) { a.Udiv(Wzr, W1, W2); } },
        { "ldr x0, [x1, #8]", Encode::Ldr(X0, Mem(X1, 8)), [](Assembler<CodeStorage>& a) { a.Ldr(X0, Mem(X1, 8)); } },
        { "ldr w0, [sp, #4]", Encode::Ldr(W0, Mem(Sp, 4)), [](Assembler<CodeStorage>& a) { a.Ldr(W0, Mem(Sp, 4)); } },
        { "str xzr, [x1, #24]",
          Encode::Str(Xzr, Mem(X1, 24)),
          [](Assembler<CodeStorage>& a) { a.Str(Xzr, Mem(X1, 24)); } },
        { "ldr x0, [x1, x2]", Encode::Ldr(X0, Mem(X1, X2)), [](Assembler<CodeStorage>& a) { a.Ldr(X0, Mem(X1, X2)); } },
        { "str xzr, [sp, x2, lsl #3]",
          Encode::Str(Xzr, Mem(Sp, X2, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Str(Xzr, Mem(Sp, X2, Scaled)); } },
        { "ldr x0, [x1, x2, lsl #3]",
          Encode::Ldr(X0, Mem(X1, X2, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Ldr(X0, Mem(X1, X2, Scaled)); } },
        { "ldr w0, [x1, w2, uxtw #2]",
          Encode::Ldr(W0, Mem(X1, W2, IndexExtend::Uxtw, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Ldr(W0, Mem(X1, W2, IndexExtend::Uxtw, Scaled)); } },
        { "str wzr, [x1, wzr, sxtw]",
          Encode::Str(Wzr, Mem(X1, Wzr, IndexExtend::Sxtw)),
          [](Assembler<CodeStorage>& a) { a.Str(Wzr, Mem(X1, Wzr, IndexExtend::Sxtw)); } },
        { "ldr xzr, [x1, x2, sxtx #3]",
          Encode::Ldr(Xzr, Mem(X1, X2, IndexExtend::Sxtx, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Ldr(Xzr, Mem(X1, X2, IndexExtend::Sxtx, Scaled)); } },
        { "ldrb w0, [x1, x2]",
          Encode::Ldrb(W0, Mem(X1, X2)),
          [](Assembler<CodeStorage>& a) { a.Ldrb(W0, Mem(X1, X2)); } },
        { "strh wzr, [sp, #2]",
          Encode::Strh(Wzr, Mem(Sp, 2)),
          [](Assembler<CodeStorage>& a) { a.Strh(Wzr, Mem(Sp, 2)); } },
        { "ldrsb x0, [x1, wzr, sxtw]",
          Encode::Ldrsb(X0, Mem(X1, Wzr, IndexExtend::Sxtw)),
          [](Assembler<CodeStorage>& a) { a.Ldrsb(X0, Mem(X1, Wzr, IndexExtend::Sxtw)); } },
        { "ldrsh w0, [x1, xzr, lsl #1]",
          Encode::Ldrsh(W0, Mem(X1, Xzr, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Ldrsh(W0, Mem(X1, Xzr, Scaled)); } },
        { "ldrsw x0, [sp, xzr, lsl #2]",
          Encode::Ldrsw(X0, Mem(Sp, Xzr, Scaled)),
          [](Assembler<CodeStorage>& a) { a.Ldrsw(X0, Mem(Sp, Xzr, Scaled)); } },
        { "ldur x0, [x1, #-8]",
          Encode::Ldur(X0, Mem(X1, -8)),
          [](Assembler<CodeStorage>& a) { a.Ldur(X0, Mem(X1, -8)); } },
        { "stur wzr, [x1, #12]",
          Encode::Stur(Wzr, Mem(X1, 12)),
          [](Assembler<CodeStorage>& a) { a.Stur(Wzr, Mem(X1, 12)); } },
        { "ldurb wzr, [sp, #-1]",
          Encode::Ldurb(Wzr, Mem(Sp, -1)),
          [](Assembler<CodeStorage>& a) { a.Ldurb(Wzr, Mem(Sp, -1)); } },
        { "ldursw x0, [x1, #255]",
          Encode::Ldursw(X0, Mem(X1, 255)),
          [](Assembler<CodeStorage>& a) { a.Ldursw(X0, Mem(X1, 255)); } },
        { "stp xzr, xzr, [sp, #-16]",
          Encode::Stp(Xzr, Xzr, Mem(Sp, -16)),
          [](Assembler<CodeStorage>& a) { a.Stp(Xzr, Xzr, Mem(Sp, -16)); } },
        { "ldp w0, wzr, [x1, #4]",
          Encode::Ldp(W0, Wzr, Mem(X1, 4)),
          [](Assembler<CodeStorage>& a) { a.Ldp(W0, Wzr, Mem(X1, 4)); } },
        { "ldpsw x0, xzr, [x1, #4]",
          Encode::Ldpsw(X0, Xzr, Mem(X1, 4)),
          [](Assembler<CodeStorage>& a) { a.Ldpsw(X0, Xzr, Mem(X1, 4)); } },
        { "movz x0, #0x1234", Encode::Movz(X0, 0x1234, 0), [](Assembler<CodeStorage>& a) { a.Movz(X0, 0x1234); } },
        { "movz w0, #0x8000, lsl #16",
          Encode::Movz(W0, 0x8000, 16),
          [](Assembler<CodeStorage>& a) { a.Movz(W0, 0x8000, 16); } },
        { "movz xzr, #0xffff, lsl #48",
          Encode::Movz(Xzr, 0xFFFF, 48),
          [](Assembler<CodeStorage>& a) { a.Movz(Xzr, 0xFFFF, 48); } },
        { "movn x0, #0x1234, lsl #32",
          Encode::Movn(X0, 0x1234, 32),
          [](Assembler<CodeStorage>& a) { a.Movn(X0, 0x1234, 32); } },
        { "movn wzr, #0", Encode::Movn(Wzr, 0, 0), [](Assembler<CodeStorage>& a) { a.Movn(Wzr, 0); } },
        { "movk w0, #0xabcd", Encode::Movk(W0, 0xABCD, 0), [](Assembler<CodeStorage>& a) { a.Movk(W0, 0xABCD); } },
        { "movk x0, #0, lsl #16", Encode::Movk(X0, 0, 16), [](Assembler<CodeStorage>& a) { a.Movk(X0, 0, 16); } },
        { "mov x0, x1", Encode::Orr(X0, Xzr, X1), [](Assembler<CodeStorage>& a) { a.Mov(X0, X1); } },
        { "mov w0, w1", Encode::Orr(W0, Wzr, W1), [](Assembler<CodeStorage>& a) { a.Mov(W0, W1); } },
        { "mov x0, sp", Encode::Add(X0, Sp, 0), [](Assembler<CodeStorage>& a) { a.Mov(X0, Sp); } },
        { "mov sp, x1", Encode::Add(Sp, X1, 0), [](Assembler<CodeStorage>& a) { a.Mov(Sp, X1); } },
        { "mov x0, #0x1234", Encode::Movz(X0, 0x1234, 0), [](Assembler<CodeStorage>& a) { a.Mov(X0, 0x1234); } },
        { "mov x0, #0xffffffffffffffff",
          Encode::Movn(X0, 0, 0),
          [](Assembler<CodeStorage>& a) { a.Mov(X0, ~uint64_t { 0 }); } },
        { "mov w0, #0", Encode::Movz(W0, 0, 0), [](Assembler<CodeStorage>& a) { a.Mov(W0, 0u); } },
        { "br x1", Encode::Br(X1), [](Assembler<CodeStorage>& a) { a.Br(X1); } },
        { "blr x30", Encode::Blr(X30), [](Assembler<CodeStorage>& a) { a.Blr(X30); } },
        { "ret x1", Encode::Ret(X1), [](Assembler<CodeStorage>& a) { a.Ret(X1); } },
        { "nop", Encode::Nop(), [](Assembler<CodeStorage>& a) { a.Nop(); } },
    };

    for (const Sample& sample : samples) {
        Assembler<CodeStorage> assembler;
        sample.emit(assembler);
        ASSERT_EQ(WordCount(assembler.Bytes()), 1u) << sample.text;
        ASSERT_EQ(WordAt(assembler.Bytes(), 0), sample.encoded.Bits()) << sample.text;
    }
}

// The immediate MOV is the one public method that emits more than one word, so
// it gets its own check: which of MOVZ and MOVN the value costs fewer words in,
// which halfword slots each of them writes, and the order they come in.
TEST(AssemblerMethods, ChoosesTheFewestMoveWideWords)
{
    struct Sample {
        const char* text;
        bool word; // the 32-bit form
        uint64_t imm;
        std::vector<uint32_t> words;
    };

    const Sample samples[] = {
        { "mov x0, #0", false, 0, { Encode::Movz(X0, 0, 0).Bits() } },
        { "mov x0, #0x1234", false, 0x1234, { Encode::Movz(X0, 0x1234, 0).Bits() } },
        { "mov x0, #0x1234000000000000", false, 0x1234ull << 48, { Encode::Movz(X0, 0x1234, 48).Bits() } },
        { "mov x0, #0xffffffffffffffff", false, ~uint64_t { 0 }, { Encode::Movn(X0, 0, 0).Bits() } },
        { "mov x0, #0xffffffffffffedcb", // every halfword but the low one is ones
          false,
          ~uint64_t { 0x1234 },
          { Encode::Movn(X0, 0x1234, 0).Bits() } },
        { "mov x0, #0x1234ffffffffffff", // the same, in the high halfword
          false,
          (0x1234ull << 48) | 0x0000FFFFFFFFFFFFull,
          { Encode::Movn(X0, ~0x1234 & 0xFFFF, 48).Bits() } },
        { "mov x0, #0xffff0000ffff0000", // MOVZ leaves out the zero halfwords
          false,
          0xFFFF0000FFFF0000ull,
          { Encode::Movz(X0, 0xFFFF, 16).Bits(), Encode::Movk(X0, 0xFFFF, 48).Bits() } },
        { "mov x0, #0x123400000000ffff", // and writes the ones it cannot leave out
          false,
          0x123400000000FFFFull,
          { Encode::Movz(X0, 0xFFFF, 0).Bits(), Encode::Movk(X0, 0x1234, 48).Bits() } },
        { "mov x0, #0x123456789abcdef0",
          false,
          0x123456789ABCDEF0ull,
          { Encode::Movz(X0, 0xDEF0, 0).Bits(),
            Encode::Movk(X0, 0x9ABC, 16).Bits(),
            Encode::Movk(X0, 0x5678, 32).Bits(),
            Encode::Movk(X0, 0x1234, 48).Bits() } },
        { "mov w0, #0", true, 0, { Encode::Movz(W0, 0, 0).Bits() } },
        { "mov w0, #0xffff", true, 0xFFFF, { Encode::Movz(W0, 0xFFFF, 0).Bits() } },
        { "mov w0, #0xffffffff", true, ~uint32_t { 0 }, { Encode::Movn(W0, 0, 0).Bits() } },
        { "mov w0, #0x12345678",
          true,
          0x12345678,
          { Encode::Movz(W0, 0x5678, 0).Bits(), Encode::Movk(W0, 0x1234, 16).Bits() } },
    };

    for (const Sample& sample : samples) {
        Assembler<CodeStorage> assembler;
        if (sample.word) {
            assembler.Mov(W0, static_cast<uint32_t>(sample.imm));
        } else {
            assembler.Mov(X0, sample.imm);
        }
        const Utils::Span<const uint8_t> bytes = assembler.Bytes();
        ASSERT_EQ(WordCount(bytes), sample.words.size()) << sample.text;
        for (size_t i = 0; i < sample.words.size(); ++i) {
            ASSERT_EQ(WordAt(bytes, i), sample.words[i]) << sample.text << " word " << i;
        }
    }
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

TEST(EmittedCode, MultipliesAddsAndSubtracts)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    {
        // mul: the product, plus the third operand, into a separate destination
        // so a misplaced source or destination register changes the result.
        Assembler<CodeStorage> assembler;
        assembler.Mul(X3, X0, X1);
        assembler.Add(X0, X3, X2);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto multiply             = reinterpret_cast<TernaryFunction>(segment.Code());
        ASSERT_EQ(multiply(6, 7, 5), 47); // 6 * 7 + 5
        ASSERT_EQ(multiply(-6, 7, 5), -37);
    }
    {
        // madd: the product plus the fourth operand.
        Assembler<CodeStorage> assembler;
        assembler.Madd(X3, X0, X1, X2);
        assembler.Add(X0, X3, X2);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto multiplyAdd          = reinterpret_cast<TernaryFunction>(segment.Code());
        ASSERT_EQ(multiplyAdd(6, 7, 5), 52); // 6 * 7 + 5, plus 5 again
        ASSERT_EQ(multiplyAdd(-6, 7, 5), -32);
    }
    {
        // msub subtracts the product from the fourth operand, not the other way
        // around: 5 - 6 * 7, plus 5 again.
        Assembler<CodeStorage> assembler;
        assembler.Msub(X3, X0, X1, X2);
        assembler.Add(X0, X3, X2);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto multiplySub          = reinterpret_cast<TernaryFunction>(segment.Code());
        ASSERT_EQ(multiplySub(6, 7, 5), -32);
        ASSERT_EQ(multiplySub(-6, 7, 5), 52);
    }
    {
        // The destination holds the low half of the exact product: 2^40 squared
        // is past the 64 bits it has.
        Assembler<CodeStorage> assembler;
        assembler.Mul(X2, X0, X0);
        assembler.Add(X0, X2, X1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto square               = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(square(int64_t { 1 } << 40, 5), 5);
    }
    {
        // The 32-bit form multiplies the low words only.
        Assembler<CodeStorage> assembler;
        assembler.Mul(W2, W0, W1);
        assembler.Add(W0, W2, W1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto multiplyWord         = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(multiplyWord(0x100000001, 3), 6); // 1 * 3 + 3
    }
#endif
}

TEST(EmittedCode, DividesTruncatingTowardZero)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    {
        Assembler<CodeStorage> assembler;
        assembler.Sdiv(X0, X0, X1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto divideSigned         = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(divideSigned(7, 2), 3);
        ASSERT_EQ(divideSigned(-7, 2), -3); // truncated toward zero, not floored
        ASSERT_EQ(divideSigned(7, -2), -3);
        ASSERT_EQ(divideSigned(-7, -2), 3);
        ASSERT_EQ(divideSigned(0, 5), 0);
        ASSERT_EQ(divideSigned(5, 0), 0); // the architecture defines division by zero as zero
        // The one overflowing quotient: the result is the dividend, not a trap.
        const int64_t lowest = static_cast<int64_t>(uint64_t { 1 } << 63);
        ASSERT_EQ(divideSigned(lowest, -1), lowest);
    }
    {
        Assembler<CodeStorage> assembler;
        assembler.Udiv(X0, X0, X1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto divideUnsigned       = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(divideUnsigned(7, 2), 3);
        // -1 as an unsigned value: the quotient has its top bit set.
        ASSERT_EQ(divideUnsigned(-1, 2), static_cast<int64_t>(~uint64_t { 0 } / 2));
        ASSERT_EQ(divideUnsigned(-7, 2), static_cast<int64_t>((~uint64_t { 0 } - 6) / 2));
        ASSERT_EQ(divideUnsigned(5, 0), 0);
    }
    {
        // The 32-bit form divides the low words: the upper halves of both
        // operands are ignored and the quotient is one.
        Assembler<CodeStorage> assembler;
        assembler.Udiv(W2, W0, W1);
        assembler.Add(W0, W2, W1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto divideWord           = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(divideWord(0x100000001, 3), 3); // 1 / 3 + 3
        ASSERT_EQ(divideWord(-1, 2), 0x80000001); // 0xFFFFFFFF / 2, plus 2
    }
#endif
}

TEST(EmittedCode, ExecutesMovesOfImmediates)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    // Values that take each of the move-wide shapes: all zero halfwords, one
    // halfword set, the top halfword set, an all-ones one, a value that MOVN
    // covers by complementing a single halfword, and one that needs all four.
    const uint64_t immediates[] = {
        0,
        1,
        0x1234,
        0x8000,
        uint64_t { 1 } << 63,
        ~uint64_t { 0 },
        ~uint64_t { 0x1234 },
        0xFFFF0000FFFF0000ull,
        0x123400000000FFFFull,
        0x123456789ABCDEF0ull,
    };

    for (uint64_t imm : immediates) {
        Assembler<CodeStorage> assembler;
        assembler.Mov(X0, imm);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto move                 = reinterpret_cast<NullaryFunction>(segment.Code());
        ASSERT_EQ(move(), imm) << "0x" << std::hex << imm;
    }

    // The 32-bit form writes the low half of the register and zeroes the upper
    // one, so these come back zero-extended even when the value is negative.
    const uint32_t immediates32[] = { 0, 0x12345678, 0x80000000, 0xFFFFFFFF };
    for (uint32_t imm : immediates32) {
        Assembler<CodeStorage> assembler;
        assembler.Mov(W0, imm);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto move                 = reinterpret_cast<NullaryFunction>(segment.Code());
        ASSERT_EQ(move(), imm) << "0x" << std::hex << imm;
    }
#endif
}

TEST(EmittedCode, MovesBetweenRegistersAndTheStackPointer)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    {
        // mov sp, x0 followed by mov x0, sp has to round-trip, and the saved
        // stack pointer has to come back untouched: the code only ever reads
        // and restores it, so a wrong move would leave the function returning
        // too far down the stack and never reaching Ret.
        Assembler<CodeStorage> assembler;
        assembler.Mov(X1, Sp); // the stack pointer as it came in
        assembler.Mov(Sp, X0); // the value passed in
        assembler.Mov(X2, Sp); // read back through SP
        assembler.Mov(Sp, X1); // and restored before the return
        assembler.Mov(X0, X2);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto roundTrip            = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(roundTrip(0x1234, 0), 0x1234);
        uint64_t local = 0;
        ASSERT_EQ(
            roundTrip(static_cast<int64_t>(reinterpret_cast<uintptr_t>(&local)), 0),
            static_cast<int64_t>(reinterpret_cast<uintptr_t>(&local))
        );
    }
    {
        // The register form is ORR with a zero source register, so the value
        // comes from the register named second and the zero register produces
        // nothing. All three operands take part — x3 = the second, then the sum
        // of all three — so a swap of the two registers changes the result.
        Assembler<CodeStorage> assembler;
        assembler.Mov(X3, X1);
        assembler.Add(X0, X0, X3);
        assembler.Add(X0, X0, X2);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto sum                  = reinterpret_cast<TernaryFunction>(segment.Code());
        ASSERT_EQ(sum(20, 22, 100), 142);
        ASSERT_EQ(sum(-20, 22, 100), 102);
    }
    {
        // The 32-bit register form moves the low word and zeroes the upper one:
        // the second operand contributes its low half only, and the sum comes
        // back zero-extended.
        Assembler<CodeStorage> assembler;
        assembler.Mov(W0, W1);
        assembler.Add(W0, W0, W1);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto twice                = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(twice(0x15, 3), 6);
        ASSERT_EQ(twice(0, 0xFFFFFFFF), 0xFFFFFFFE);
        ASSERT_EQ(twice(0x100000015, 3), 6); // the upper half of the source is ignored
    }
#endif
}

// The load and store family: the widths, the address forms and the operand
// registers that separate its encodings, each exercised by a short program.
TEST(EmittedCode, StoresAndLoadsWords)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Str(X1, Mem(X0, 0)); // the second argument into the address in the first
    assembler.Ldr(X0, Mem(X0, 0)); // and back out as the result
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto storeAndLoad         = reinterpret_cast<BinaryFunction>(segment.Code());

    uint64_t slot = 0;
    ASSERT_EQ(storeAndLoad(reinterpret_cast<int64_t>(&slot), 0x0123456789ABCDEF), 0x0123456789ABCDEF);
    ASSERT_EQ(slot, 0x0123456789ABCDEFull);

    ASSERT_EQ(storeAndLoad(reinterpret_cast<int64_t>(&slot), -1), -1);
    ASSERT_EQ(slot, ~uint64_t { 0 });
#endif
}

TEST(EmittedCode, StoresNarrowValuesWithoutClobberingTheirNeighbours)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Strb(W1, Mem(X0, 0)); // one byte of the second argument
    assembler.Strh(W1, Mem(X0, 2)); // a halfword, two bytes above it
    assembler.Str(W1, Mem(X0, 4));  // and a whole word, four bytes above that
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto store                = reinterpret_cast<BinaryFunction>(segment.Code());

    // -1 makes every bit of the argument one, so bytes a store writes past its
    // own width would show up in the check below.
    uint8_t slot[8] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
    (void)store(reinterpret_cast<int64_t>(slot), -1);
    const uint8_t expected[8] = { 0xFF, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    for (size_t i = 0; i < sizeof(expected); ++i) {
        ASSERT_EQ(slot[i], expected[i]) << "byte " << i;
    }
#endif
}

TEST(EmittedCode, LoadsSignExtendedValues)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    // The byte at offset 0 is 0x80, the halfword at offset 2 is 0x8001 and the
    // word at offset 4 is 0x80000001: each value has its sign bit set.
    uint64_t slot    = 0;
    uint8_t* bytes   = reinterpret_cast<uint8_t*>(&slot);
    bytes[0]         = 0x80;
    bytes[1]         = 0xAA;
    bytes[2]         = 0x01;
    bytes[3]         = 0x80;
    bytes[4]         = 0x01;
    bytes[5]         = 0x00;
    bytes[6]         = 0x00;
    bytes[7]         = 0x80;
    const int64_t at = reinterpret_cast<int64_t>(&slot);

    {
        Assembler<CodeStorage> assembler;
        assembler.Ldrsb(X0, Mem(X0, 0));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadSignedByte       = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadSignedByte(at, 0), -128);
    }
    {
        Assembler<CodeStorage> assembler;
        assembler.Ldrb(W0, Mem(X0, 0));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadByte             = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadByte(at, 0), 0x80);
    }
    {
        Assembler<CodeStorage> assembler;
        assembler.Ldrsh(X0, Mem(X0, 2));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadSignedHalfword   = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadSignedHalfword(at, 0), -32767);
    }
    {
        Assembler<CodeStorage> assembler;
        assembler.Ldrh(W0, Mem(X0, 2));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadHalfword         = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadHalfword(at, 0), 0x8001);
    }
    {
        Assembler<CodeStorage> assembler;
        assembler.Ldrsw(X0, Mem(X0, 4));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadSignedWord       = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadSignedWord(at, 0), -2147483647);
    }
    {
        // The pair form reads two words and extends both.
        uint32_t pair[2] = { 0xFFFFFFFF, 2 };
        Assembler<CodeStorage> assembler;
        assembler.Ldpsw(X2, X3, Mem(X0, 0));
        assembler.Add(X0, X2, X3);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto loadSignedPair       = reinterpret_cast<BinaryFunction>(segment.Code());
        ASSERT_EQ(loadSignedPair(reinterpret_cast<int64_t>(pair), 0), 1);
    }
#endif
}

TEST(EmittedCode, IndexesMemoryThroughTheScaledAndUnscaledOffset)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    const auto fill = [](uint64_t(&buffer)[4]) {
        for (uint64_t& word : buffer) {
            word = 0xAAAAAAAAAAAAAAAAull;
        }
    };

    // The scaled form takes the index in 8-byte elements: element 1 is byte 8.
    {
        Assembler<CodeStorage> assembler;
        assembler.Str(X1, Mem(X0, X2, Scaled));
        assembler.Ldr(X0, Mem(X0, X2, Scaled));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto storeScaled          = reinterpret_cast<TernaryFunction>(segment.Code());

        uint64_t buffer[4];
        fill(buffer);
        ASSERT_EQ(storeScaled(reinterpret_cast<int64_t>(buffer), 0x1111, 1), 0x1111);
        ASSERT_EQ(buffer[0], 0xAAAAAAAAAAAAAAAAull);
        ASSERT_EQ(buffer[1], 0x1111ull);
        ASSERT_EQ(buffer[2], 0xAAAAAAAAAAAAAAAAull);
    }

    // The unscaled form takes the same index in bytes, so 8 reaches that byte.
    {
        Assembler<CodeStorage> assembler;
        assembler.Str(X1, Mem(X0, X2));
        assembler.Ldr(X0, Mem(X0, X2));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto storeByteOffset      = reinterpret_cast<TernaryFunction>(segment.Code());

        uint64_t buffer[4];
        fill(buffer);
        ASSERT_EQ(storeByteOffset(reinterpret_cast<int64_t>(buffer), 0x2222, 8), 0x2222);
        ASSERT_EQ(buffer[1], 0x2222ull);
    }

    // The same form at byte 1 stores the whole register there, crossing into the
    // second word: the bytes the store does not reach keep their value.
    {
        Assembler<CodeStorage> assembler;
        assembler.Str(X1, Mem(X0, X2));
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto storeByteOffset      = reinterpret_cast<TernaryFunction>(segment.Code());

        uint64_t buffer[4];
        fill(buffer);
        (void)storeByteOffset(reinterpret_cast<int64_t>(buffer), 0x33, 1);
        ASSERT_EQ(buffer[0], 0x33AAull);
        ASSERT_EQ(buffer[1], 0xAAAAAAAAAAAAAA00ull);
    }
#endif
}

TEST(EmittedCode, IndexesMemoryThroughAWordIndex)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    // The index is read as a 32-bit value and scaled by the width read: a word.
    assembler.Ldr(W0, Mem(X0, W1, IndexExtend::Uxtw, Scaled));
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto loadWord             = reinterpret_cast<BinaryFunction>(segment.Code());

    const uint32_t words[4] = { 10, 20, 30, 40 };
    const int64_t at        = reinterpret_cast<int64_t>(words);
    ASSERT_EQ(loadWord(at, 0), 10);
    ASSERT_EQ(loadWord(at, 1), 20);
    ASSERT_EQ(loadWord(at, 2), 30);
    // Only the low 32 bits of the index take part: as a 64-bit value this index
    // is far past the end, as a 32-bit one it is 1.
    ASSERT_EQ(loadWord(at, 0x100000001), 20);
#endif
}

TEST(EmittedCode, StoresAndLoadsPairs)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    {
        Assembler<CodeStorage> assembler;
        assembler.Stp(X1, X2, Mem(X0, 0));
        assembler.Ldp(X3, X4, Mem(X0, 0));
        assembler.Sub(X0, X3, X4); // the difference proves both pair slots reloaded
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto storePair            = reinterpret_cast<TernaryFunction>(segment.Code());

        uint64_t pair[2] = { 0, 0 };
        ASSERT_EQ(storePair(reinterpret_cast<int64_t>(pair), 0x1234, 0x111), 0x1234 - 0x111);
        ASSERT_EQ(pair[0], 0x1234ull);
        ASSERT_EQ(pair[1], 0x111ull);
    }
    {
        // The 32-bit form stores half as much per slot.
        Assembler<CodeStorage> assembler;
        assembler.Stp(W1, W2, Mem(X0, 0));
        assembler.Ldp(W3, W4, Mem(X0, 0));
        assembler.Sub(W0, W3, W4);
        assembler.Ret();
        const CodeSegment segment = assembler.Finalize();
        auto storePair            = reinterpret_cast<TernaryFunction>(segment.Code());

        uint32_t pair[2] = { 0, 0 };
        ASSERT_EQ(storePair(reinterpret_cast<int64_t>(pair), 0x100000003, 1), 2);
        ASSERT_EQ(pair[0], 3u);
        ASSERT_EQ(pair[1], 1u);
    }
#endif
}

TEST(EmittedCode, KeepsTheStackPointerAndTheZeroRegister)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Sub(Sp, Sp, 32);      // room for two words on the stack
    assembler.Str(X1, Mem(Sp, 16)); // the second argument in one
    assembler.Str(Xzr, Mem(Sp, 8)); // and the zero register in the other
    assembler.Ldr(X0, Mem(Sp, 16));
    assembler.Ldr(X2, Mem(Sp, 8));
    assembler.Add(X0, X0, X2); // adding zero changes nothing
    assembler.Add(Sp, Sp, 32); // give the room back
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto loadFromStack        = reinterpret_cast<BinaryFunction>(segment.Code());

    // The first argument is unused: the program works through the stack pointer.
    ASSERT_EQ(loadFromStack(0, 0x1234), 0x1234);
    ASSERT_EQ(loadFromStack(0, -1), -1);
#endif
}

TEST(EmittedCode, StoresAndLoadsWithUnscaledOffsets)
{
#if !defined(__aarch64__)
    GTEST_SKIP() << "Running emitted code requires an aarch64 host";
#else
    Assembler<CodeStorage> assembler;

    assembler.Stur(X1, Mem(X0, 8));   // one word above the address in the first argument
    assembler.Stur(Xzr, Mem(X0, -8)); // and zero one word below it
    assembler.Ldur(X2, Mem(X0, 8));
    assembler.Ldur(X3, Mem(X0, -8));
    assembler.Add(X0, X2, X3); // the zero comes back in unchanged
    assembler.Ret();
    const CodeSegment segment = assembler.Finalize();
    auto storeUnscaled        = reinterpret_cast<BinaryFunction>(segment.Code());

    uint64_t words[3] = { 0xAAAAAAAAAAAAAAAAull, 0xAAAAAAAAAAAAAAAAull, 0xAAAAAAAAAAAAAAAAull };
    ASSERT_EQ(storeUnscaled(reinterpret_cast<int64_t>(&words[1]), 0x5678), 0x5678);
    ASSERT_EQ(words[0], 0ull);
    ASSERT_EQ(words[1], 0xAAAAAAAAAAAAAAAAull);
    ASSERT_EQ(words[2], 0x5678ull);
#endif
}
