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
    // The mock runs an I2I callee with frameBot = 0, so truly stack-passed
    // params (staged by st.stack.param.*) would be stored to absolute low
    // addresses; this suite keeps every signature inside the 6-int/8-float
    // register windows (src/cbc/abi.h).
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
