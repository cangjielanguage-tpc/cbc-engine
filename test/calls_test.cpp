#include "cbc_test_harness.h"

TEST_ASM(CbcTest, CallIntParams)
{
    auto code = OpenAndRewrite("main", "./generated-asm/calls/sum6i.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, 6 * x + 15);
    }
}

TEST_ASM(CbcTest, CallStackParams)
{
    // Mixed int/float params, all inside the 6-int/8-float register windows
    // (src/cbc/abi.h), so the default frameBot = 0 suffices (no staging).
    auto code = OpenAndRewrite("main", "./generated-asm/calls/sum-mixed.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, 6 * x + 115);
    }
}

TEST_ASM(CbcTest, CallFloatParams)
{
    {
        auto code = OpenAndRewrite("main", "./generated-asm/calls/fsum8.asm")->code;
        EXPECT_EQ(InterpretFPRes(code, F64(0), F64(0)).f64, 36.0);
    }
    {
        auto code = OpenAndRewrite("main", "./generated-asm/calls/fsum4w.asm")->code;
        EXPECT_EQ(InterpretFPRes(code, F32(0), F32(0)).f32, 38.5f);
    }
}

TEST_ASM(CbcTest, CallArgsFromSlots)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };

    auto code = OpenAndRewrite("main", "./generated-asm/calls/args-from-slots.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, frame, U64(x), U64(0)).u64, x + 7);
    }
}

TEST_ASM(CbcTest, CallShuffledArgs)
{
    auto code = OpenAndRewrite("main", "./generated-asm/calls/shuffle.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, 273 * x + 528);
    }
}

TEST_ASM(CbcTest, CallNested)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };

    auto code = OpenAndRewrite("main", "./generated-asm/calls/nested.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, frame, U64(x), U64(0)).u64, 4 * x - 2);
    }
}

TEST_ASM(CbcTest, CallTwice)
{
    auto code = OpenAndRewrite("main", "./generated-asm/calls/twocalls.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, 8 * x + 2);
    }
}

TEST_ASM(CbcTest, CallNonVolPreserved)
{
    auto code = OpenAndRewrite("main", "./generated-asm/calls/nonvol.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, x + 0xff);
    }
}

// Stack-passed call arguments are staged by the rewriter at frameBot + k * STACK_SLOT_SIZE
// (st.stack.param*), where frameBot = frame.start - ExecBytecodeInfo::frameSize. The tests
// below pass that real frameBot to the mock and read the staged cells back from the frame
// buffer; the callees cannot read stack params (no IR7 operands in the DSL), so they only
// consume the 6-int/8-float register window.

TEST_ASM(CbcTest, CallStackIntParams)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    for (auto& slot : frameSlots) {
        slot = 0xbad0bad0bad0bad0ull;
    }
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };

    auto info     = OpenAndRewrite("main", "./generated-asm/calls/sum8i.asm");
    auto frameBot = frameStart - info->frameSize;
    auto code     = info->code;
    auto staged   = reinterpret_cast<const uint64_t*>(frameBot);

    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, frame, U64(x), U64(0), frameBot).u64, 6 * x + 15);
        EXPECT_EQ(staged[0], x + 6);
        EXPECT_EQ(staged[1], x + 7);
    }
}

TEST_ASM(CbcTest, CallStackFloatParams)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    for (auto& slot : frameSlots) {
        slot = 0xbad0bad0bad0bad0ull;
    }
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };

    auto info     = OpenAndRewrite("main", "./generated-asm/calls/fsum10f.asm");
    auto frameBot = frameStart - info->frameSize;
    auto code     = info->code;
    auto staged   = reinterpret_cast<const double*>(frameBot);

    EXPECT_EQ(InterpretFPRes(code, frame, F64(0), F64(0), frameBot).f64, 36.0);
    EXPECT_EQ(staged[0], 9.0);
    EXPECT_EQ(staged[1], 10.0);
}

TEST_ASM(CbcTest, CallStackSlotParams)
{
    // The staged args are sourced from the caller's frame slots (st.stack.param.s.16).
    // The frame region is placed inside a larger buffer with sentinel-filled slack below
    // it, proving that both the slot writes and the staging stay inside the provided
    // buffer and never reach below frameBot.
    constexpr auto stackSlotCount = 100;
    constexpr auto slackSlots     = 8;
    constexpr uint64_t sentinel   = 0xbad0bad0bad0bad0ull;
    uint64_t buf[stackSlotCount + slackSlots];
    for (auto& slot : buf) {
        slot = sentinel;
    }
    auto frameStart = reinterpret_cast<uintptr_t>(buf + stackSlotCount + slackSlots);
    Interpretation::Frame frame { frameStart };

    auto info     = OpenAndRewrite("main", "./generated-asm/calls/sum8i-slots.asm");
    auto frameBot = frameStart - info->frameSize;
    ASSERT_GT(frameBot, reinterpret_cast<uintptr_t>(buf));
    ASSERT_LT(frameBot, frameStart);

    auto code   = info->code;
    auto staged = reinterpret_cast<const uint64_t*>(frameBot);

    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, frame, U64(x), U64(0), frameBot).u64, 6 * x + 15);
        EXPECT_EQ(staged[0], x + 6);
        EXPECT_EQ(staged[1], x + 7);
        for (auto slot = buf; slot < reinterpret_cast<const uint64_t*>(frameBot); ++slot) {
            ASSERT_EQ(*slot, sentinel);
        }
    }
}

TEST_ASM(CbcTest, CallStackWideSlotParams)
{
    // staticFrameSize above 32K makes the 16-bit source offset overflow, so the
    // slot -> staged-cell copy is encoded with st.stack.param.s.32.
    constexpr auto stackSlotCount = 100;
    constexpr auto wideSlotCount  = 5000;
    uint64_t frameSlots[stackSlotCount + wideSlotCount];
    for (auto& slot : frameSlots) {
        slot = 0xbad0bad0bad0bad0ull;
    }
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount + wideSlotCount);
    Interpretation::Frame frame { frameStart };

    auto info     = OpenAndRewrite("main", "./generated-asm/calls/sum8i-slots-wide.asm");
    auto frameBot = frameStart - info->frameSize;
    auto code     = info->code;
    auto staged   = reinterpret_cast<const uint64_t*>(frameBot);

    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, frame, U64(x), U64(0), frameBot).u64, 6 * x + 15);
        EXPECT_EQ(staged[0], x + 6);
        EXPECT_EQ(staged[1], x + 7);
    }
}

TEST_ASM(CbcTest, CallFullRegWindows)
{
    auto code = OpenAndRewrite("main", "./generated-asm/calls/sum6i8f.asm")->code;
    for (uint64_t x : {1ull, 7ull, 0x1234ull}) {
        SCOPED_TRACE("x=" + hex(x));
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, 6 * x + 51);
    }
}
