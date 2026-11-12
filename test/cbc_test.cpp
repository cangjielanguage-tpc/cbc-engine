#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "engine/image/io/byte_array_random_access_file.h"
#include "engine/image/reader.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "utils/vector.h"

#include "interpreter/loggers.h"
#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

static void DoSetUp()
{
    Cbc::EnableRawDisasm();
    InitializeMockInterpreter();
    heap.Reset();
}

class CbcTest : public testing::Test {
    void SetUp() override
    {
        Interpretation::Log::preparation.SetLogLevel(Logging::Level::TRACE);
        Cbc::EnableRawDisasm();
        InitializeMockInterpreter();
        heap.Reset();
    }

    void TearDown() override {}
};

static std::unique_ptr<IO::ByteArrayRandomAccessFile> FromString(std::string_view view)
{
    Utils::Vector<char> data(view.size());
    std::copy(view.begin(), view.end(), data.Data());

    return std::make_unique<IO::ByteArrayRandomAccessFile>(std::move(data));
}

TEST_F(CbcTest, Empty)
{
    GTEST_SKIP() << "WIP";
    Engine::Loader loader;
    constexpr auto buf_size = 128;
    uint8_t buf[buf_size]   = { 0xf0, 0xaf, 0xcd, 0xcb, 0 };

    constexpr uint32_t offs = 100;
    buf[offs + 0]           = 3;
    buf[offs + 4]           = 'a';
    buf[offs + 5]           = 'b';
    buf[offs + 6]           = 'c';

    std::string_view raw((char*)buf, buf_size);

    auto file       = FromString(raw);
    bool successful = loader.Load(std::move(file), "hello.cbc");
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto strOffs = Image::Offset<Image::String>(offs);
    auto str     = Decode::Read(session, Image::FileId(0), strOffs);

    ASSERT_EQ(str, "abc");
}

// Deliberately never destroyed: engines are process-wide, and the registry must outlive
// static destructors so that LeakSanitizer still sees every engine as reachable at exit.
// TODO: FIXME #182
static std::vector<Engine::Engine*>& BuiltEngines()
{
    static auto* engines = new std::vector<Engine::Engine*>;
    return *engines;
}

static Interpretation::ExecBytecodeInfo* OpenAndRewrite(std::string name, std::string_view fileName)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);

    auto& engine = loader.Build();
    BuiltEngines().push_back(&engine);
    Engine::Session session(engine);
    auto methodId    = engine.FindMethod(session, fileName, "default", name);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh).info;
}

TEST_ASM(CbcTest, Simple)
{
    auto res = Interpret(OpenAndRewrite("main", "simple.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, SimpleArith)
{
    auto res = Interpret(OpenAndRewrite("main", "simple_arith.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 36);
}

TEST_ASM(CbcTest, SimpleArithImm)
{
    auto res = Interpret(OpenAndRewrite("main", "simple_arith_imm.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 1);
}

TEST_ASM(CbcTest, FibRec)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount];
    // Frame slots are addressed negatively relative to the frame start:
    // slot N is located at frame.start + N * STACK_SLOT_SIZE - frameSize.
    auto frameStart = reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount);
    Interpretation::Frame frame { frameStart };

    auto res = Interpret(OpenAndRewrite("main", "fib-rec.asm")->code, frame, U32(0), U32(0));
    ASSERT_EQ(res.u32, 13);
}

TEST_ASM(CbcTest, FibRecRegs)
{
    auto res = Interpret(OpenAndRewrite("main", "fib-rec-regs.asm")->code, U32(0), U32(0));
    ASSERT_EQ(res.u32, 13);
}

// A frame buffer addressed the way the interpreter addresses frame slots:
// frame.start points past the last slot, every slot sits at a negative
// offset (see CbcTest.FibRec).
struct TestFrame {
    static constexpr auto stackSlotCount = 100;

    uint64_t slots[stackSlotCount];
    Interpretation::Frame frame;

    TestFrame() : frame { reinterpret_cast<uintptr_t>(slots + stackSlotCount) } {}
};

TEST_ASM(CbcTest, TypedSlotRoundTrip)
{
    TestFrame f;
    {
        auto code = OpenAndRewrite("test_typed_i64", "./generated-asm/frame-slots/typed_i64.asm")->code;
        auto res  = Interpret(code, f.frame, U64(0x2a), U64(0));
        ASSERT_EQ(res.u64, 0x2a);
    }
    {
        auto code = OpenAndRewrite("test_typed_f64", "./generated-asm/frame-slots/typed_f64.asm")->code;
        auto res  = InterpretFPRes(code, f.frame, F64(2.5), F64(0));
        ASSERT_EQ(res.u64, F64(2.5).u64);
    }
}

TEST_ASM(CbcTest, TypedSlotTwoSlots)
{
    TestFrame f;
    auto code = OpenAndRewrite("test_typed_two", "./generated-asm/frame-slots/typed_two.asm")->code;
    auto res  = Interpret(code, f.frame, U64(3), U64(4));
    ASSERT_EQ(res.u64, 7);
}

TEST_ASM(CbcTest, TypedSlotStackRec)
{
    TestFrame f;
    auto code = OpenAndRewrite("test_stack_rec", "./generated-asm/frame-slots/stack_rec.asm")->code;
    auto res  = Interpret(code, f.frame, U64(0x2a), U64(0));
    ASSERT_EQ(res.u64, 0x2a);
}

TEST_ASM(CbcTest, DirectCall)
{
    auto res = Interpret(OpenAndRewrite("main", "direct-call.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, ArithSpecialized1)
{
    auto res = Interpret(OpenAndRewrite("test_1", "generated-asm/arith-specialized/1.asm")->code, U64(10), U64(0));
    ASSERT_EQ(res.u64, 10 - 1);
}

TEST_ASM(CbcTest, ArithSpecialized2)
{
    auto res = Interpret(OpenAndRewrite("test_2", "generated-asm/arith-specialized/2.asm")->code, U64(0), U64(0));
    ASSERT_EQ(res.u64, 0x7000000000000000 ^ 0xff00);
}

static uint64_t Add(uint64_t lhs, uint64_t rhs) { return lhs + rhs; }

static uint64_t Sub(uint64_t lhs, uint64_t rhs) { return lhs - rhs; }

static uint64_t Mul(uint64_t lhs, uint64_t rhs) { return lhs * rhs; }

static uint64_t And(uint64_t lhs, uint64_t rhs) { return lhs & rhs; }

static uint64_t Or(uint64_t lhs, uint64_t rhs) { return lhs | rhs; }

static uint64_t Xor(uint64_t lhs, uint64_t rhs) { return lhs ^ rhs; }

static uint64_t UDiv(uint64_t lhs, uint64_t rhs) { return lhs / rhs; }

static uint64_t Div(uint64_t lhs, uint64_t rhs)
{
    auto left  = static_cast<int64_t>(lhs);
    auto right = static_cast<int64_t>(rhs);
    return static_cast<uint64_t>(left / right);
}

static uint64_t Rem(uint64_t lhs, uint64_t rhs)
{
    auto left  = static_cast<int64_t>(lhs);
    auto right = static_cast<int64_t>(rhs);
    return static_cast<uint64_t>(left % right);
}

static uint64_t URem(uint64_t lhs, uint64_t rhs) { return lhs % rhs; }

static uint64_t LSL(uint64_t lhs, uint64_t rhs) { return lhs << (rhs & 0x3f); }

static uint64_t LSR(uint64_t lhs, uint64_t rhs) { return lhs >> (rhs & 0x3f); }

static uint64_t ASR(uint64_t lhs, uint64_t rhs)
{
    auto left = static_cast<int64_t>(lhs);
    return static_cast<uint64_t>(left >> (rhs & 0x3f));
}

using ArithFunction = uint64_t (*)(uint64_t, uint64_t);

std::string hex(uint64_t num)
{
    std::ostringstream message;
    message << "0x" << std::hex << num;
    return message.str();
}

struct ArithTestParams {
    std::string name;
    ArithFunction arith;
};

static std::ostream& operator<<(std::ostream& os, const ArithTestParams& p) { return os << p.name; }

class CbcSpecializedArith : public ::testing::TestWithParam<ArithTestParams> {
    void SetUp() override { DoSetUp(); }
};

uint64_t rhsValues[] = {
    0x1,
    0x10,
    0x100,
    0x1020,
    0x10000,
    0x100200,
    0x1000020,
    0x7000000000000000,
    0x7000000010000001,
    0xf000100000000001,
    0xf000000100000001,
};

uint64_t lhsValues[] = {
    1, 20, 301, 402, 0x3311, 0x7222222222222222, 0xf111111111111111, 0xffffffffffffffff,
};

TEST_P(CbcSpecializedArith, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    ArithTestParams params = GetParam();

    for (auto rhs : rhsValues) {
        auto path = "./generated-asm/arith/" + params.name + "/" + hex(rhs) + ".asm";
        auto code = OpenAndRewrite("test_" + hex(rhs), path)->code;
        for (auto lhs : lhsValues) {
            auto res = Interpret(code, U64(lhs), U64(0));
            EXPECT_EQ(res.u64, params.arith(lhs, rhs));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcSpecializedArith,
    ::testing::Values(
        ArithTestParams { "Add", &Add },
        ArithTestParams { "Sub", &Sub },
        ArithTestParams { "Mul", &Mul },
        ArithTestParams { "And", &And },
        ArithTestParams { "Or", &Or },
        ArithTestParams { "Xor", &Xor },
        ArithTestParams { "UDiv", &UDiv },
        ArithTestParams { "Div", &Div },
        ArithTestParams { "Rem", &Rem },
        ArithTestParams { "URem", &URem },
        ArithTestParams { "LSL", &LSL },
        ArithTestParams { "LSR", &LSR },
        ArithTestParams { "ASR", &ASR }
    )
);

struct CastCase {
    std::string name;
    bool fromFP;
    Interpretation::Value::Primitive expected;
    Interpretation::Value::Primitive val;
};

CastCase castToIntegerCases[] = { { "U8_F32", true, U64(0), F32(0.0f) },
    { "U8_F32", true, U64(42), F32(42.75f) },
    { "U8_F32", true, U64(0), F32(-10.5f) },
    { "U8_F32", true, U64(std::numeric_limits<uint8_t>::max()), F32(std::numeric_limits<uint8_t>::max()) },
    { "U8_F32", true, U64(std::numeric_limits<uint8_t>::max()), F32(0x1p8f) },
    { "U8_F32", true, U64(std::numeric_limits<uint8_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "U8_F32", true, U64(0), F32(-std::numeric_limits<float>::infinity()) },
    { "U8_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "U8_F64", true, U64(0), F64(0.0) },
    { "U8_F64", true, U64(42), F64(42.75) },
    { "U8_F64", true, U64(0), F64(-10.5) },
    { "U8_F64", true, U64(std::numeric_limits<uint8_t>::max()), F64(std::numeric_limits<uint8_t>::max()) },
    { "U8_F64", true, U64(std::numeric_limits<uint8_t>::max()), F64(0x1p8) },
    { "U8_F64", true, U64(std::numeric_limits<uint8_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "U8_F64", true, U64(0), F64(-std::numeric_limits<double>::infinity()) },
    { "U8_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "U16_F32", true, U64(0), F32(0.0f) },
    { "U16_F32", true, U64(42), F32(42.75f) },
    { "U16_F32", true, U64(0), F32(-10.5f) },
    { "U16_F32", true, U64(std::numeric_limits<uint16_t>::max()), F32(std::numeric_limits<uint16_t>::max()) },
    { "U16_F32", true, U64(std::numeric_limits<uint16_t>::max()), F32(0x1p16f) },
    { "U16_F32", true, U64(std::numeric_limits<uint16_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "U16_F32", true, U64(0), F32(-std::numeric_limits<float>::infinity()) },
    { "U16_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "U16_F64", true, U64(0), F64(0.0) },
    { "U16_F64", true, U64(42), F64(42.75) },
    { "U16_F64", true, U64(0), F64(-10.5) },
    { "U16_F64", true, U64(std::numeric_limits<uint16_t>::max()), F64(std::numeric_limits<uint16_t>::max()) },
    { "U16_F64", true, U64(std::numeric_limits<uint16_t>::max()), F64(0x1p16) },
    { "U16_F64", true, U64(std::numeric_limits<uint16_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "U16_F64", true, U64(0), F64(-std::numeric_limits<double>::infinity()) },
    { "U16_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "U32_F32", true, U64(0), F32(0.0f) },
    { "U32_F32", true, U64(42), F32(42.75f) },
    { "U32_F32", true, U64(0), F32(-10.5f) },
    { "U32_F32", true, U64(std::numeric_limits<uint32_t>::max()), F32(std::numeric_limits<uint32_t>::max()) },
    { "U32_F32", true, U64(std::numeric_limits<uint32_t>::max()), F32(0x1p32f) },
    { "U32_F32", true, U64(std::numeric_limits<uint32_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "U32_F32", true, U64(0), F32(-std::numeric_limits<float>::infinity()) },
    { "U32_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "U32_F64", true, U64(0), F64(0.0) },
    { "U32_F64", true, U64(42), F64(42.75) },
    { "U32_F64", true, U64(0), F64(-10.5) },
    { "U32_F64", true, U64(std::numeric_limits<uint32_t>::max()), F64(std::numeric_limits<uint32_t>::max()) },
    { "U32_F64", true, U64(std::numeric_limits<uint32_t>::max()), F64(0x1p32) },
    { "U32_F64", true, U64(std::numeric_limits<uint32_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "U32_F64", true, U64(0), F64(-std::numeric_limits<double>::infinity()) },
    { "U32_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "U64_F32", true, U64(0), F32(0.0f) },
    { "U64_F32", true, U64(42), F32(42.75f) },
    { "U64_F32", true, U64(0), F32(-10.5f) },
    { "U64_F32", true, U64(std::numeric_limits<uint64_t>::max()), F32(std::numeric_limits<uint64_t>::max()) },
    { "U64_F32", true, U64(std::numeric_limits<uint64_t>::max()), F32(0x1p64f) },
    { "U64_F32", true, U64(std::numeric_limits<uint64_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "U64_F32", true, U64(0), F32(-std::numeric_limits<float>::infinity()) },
    { "U64_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "U64_F64", true, U64(0), F64(0.0) },
    { "U64_F64", true, U64(42), F64(42.75) },
    { "U64_F64", true, U64(0), F64(-10.5) },
    { "U64_F64", true, U64(std::numeric_limits<uint64_t>::max()), F64(std::numeric_limits<uint64_t>::max()) },
    { "U64_F64", true, U64(std::numeric_limits<uint64_t>::max()), F64(0x1p64) },
    { "U64_F64", true, U64(std::numeric_limits<uint64_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "U64_F64", true, U64(0), F64(-std::numeric_limits<double>::infinity()) },
    { "U64_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "I8_F32", true, U64(0), F32(0.0f) },
    { "I8_F32", true, U64(0), F32(-0.0f) },
    { "I8_F32", true, U64(42), F32(42.75f) },
    { "I8_F32", true, U64(-42), F32(-42.75f) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::max()), F32(static_cast<float>(std::numeric_limits<int8_t>::max())) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::min()), F32(static_cast<float>(std::numeric_limits<int8_t>::min())) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::max()), F32(0x1p7f) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::min()), F32(-0x1p8f) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "I8_F32", true, U64(std::numeric_limits<int8_t>::min()), F32(-std::numeric_limits<float>::infinity()) },
    { "I8_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "I8_F64", true, U64(0), F64(0.0) },
    { "I8_F64", true, U64(0), F64(-0.0) },
    { "I8_F64", true, U64(42), F64(42.75) },
    { "I8_F64", true, U64(-42), F64(-42.75) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::max()), F64(static_cast<double>(std::numeric_limits<int8_t>::max())) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::min()), F64(static_cast<double>(std::numeric_limits<int8_t>::min())) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::max()), F64(0x1p7) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::min()), F64(-0x1p8) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "I8_F64", true, U64(std::numeric_limits<int8_t>::min()), F64(-std::numeric_limits<double>::infinity()) },
    { "I8_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "I16_F32", true, U64(0), F32(0.0f) },
    { "I16_F32", true, U64(0), F32(-0.0f) },
    { "I16_F32", true, U64(42), F32(42.75f) },
    { "I16_F32", true, U64(-42), F32(-42.75f) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::max()), F32(static_cast<float>(std::numeric_limits<int16_t>::max())) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::min()), F32(static_cast<float>(std::numeric_limits<int16_t>::min())) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::max()), F32(0x1p15f) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::min()), F32(-0x1p16f) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "I16_F32", true, U64(std::numeric_limits<int16_t>::min()), F32(-std::numeric_limits<float>::infinity()) },
    { "I16_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "I16_F64", true, U64(0), F64(0.0) },
    { "I16_F64", true, U64(0), F64(-0.0) },
    { "I16_F64", true, U64(42), F64(42.75) },
    { "I16_F64", true, U64(-42), F64(-42.75) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::max()), F64(static_cast<double>(std::numeric_limits<int16_t>::max())) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::min()), F64(static_cast<double>(std::numeric_limits<int16_t>::min())) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::max()), F64(0x1p15) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::min()), F64(-0x1p16) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "I16_F64", true, U64(std::numeric_limits<int16_t>::min()), F64(-std::numeric_limits<double>::infinity()) },
    { "I16_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "I32_F32", true, U64(0), F32(0.0f) },
    { "I32_F32", true, U64(0), F32(-0.0f) },
    { "I32_F32", true, U64(42), F32(42.75f) },
    { "I32_F32", true, U64(-42), F32(-42.75f) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::max()), F32(static_cast<float>(std::numeric_limits<int32_t>::max())) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::min()), F32(static_cast<float>(std::numeric_limits<int32_t>::min())) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::max()), F32(0x1p31f) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::min()), F32(-0x1p32f) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "I32_F32", true, U64(std::numeric_limits<int32_t>::min()), F32(-std::numeric_limits<float>::infinity()) },
    { "I32_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "I32_F32", true, U64(2147483520), F32(0x1.fffffep30f) },
    { "I32_F64", true, U64(0), F64(0.0) },
    { "I32_F64", true, U64(0), F64(-0.0) },
    { "I32_F64", true, U64(42), F64(42.75) },
    { "I32_F64", true, U64(-42), F64(-42.75) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::max()), F64(static_cast<double>(std::numeric_limits<int32_t>::max())) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::min()), F64(static_cast<double>(std::numeric_limits<int32_t>::min())) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::max()), F64(0x1p31) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::min()), F64(-0x1p32) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "I32_F64", true, U64(std::numeric_limits<int32_t>::min()), F64(-std::numeric_limits<double>::infinity()) },
    { "I32_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "I64_F32", true, U64(0), F32(0.0f) },
    { "I64_F32", true, U64(0), F32(-0.0f) },
    { "I64_F32", true, U64(42), F32(42.75f) },
    { "I64_F32", true, U64(-42), F32(-42.75f) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::max()), F32(static_cast<float>(std::numeric_limits<int64_t>::max())) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::min()), F32(static_cast<float>(std::numeric_limits<int64_t>::min())) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::max()), F32(0x1p63f) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::min()), F32(-0x1p64f) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::max()), F32(std::numeric_limits<float>::infinity()) },
    { "I64_F32", true, U64(std::numeric_limits<int64_t>::min()), F32(-std::numeric_limits<float>::infinity()) },
    { "I64_F32", true, U64(0), F32(std::numeric_limits<float>::quiet_NaN()) },
    { "I64_F32", true, U64(9223371487098961920LL), F32(0x1.fffffep62f) },
    { "I64_F64", true, U64(0), F64(0.0) },
    { "I64_F64", true, U64(0), F64(-0.0) },
    { "I64_F64", true, U64(42), F64(42.75) },
    { "I64_F64", true, U64(-42), F64(-42.75) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::max()), F64(static_cast<double>(std::numeric_limits<int64_t>::max())) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::min()), F64(static_cast<double>(std::numeric_limits<int64_t>::min())) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::max()), F64(0x1p63) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::min()), F64(-0x1p64) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::max()), F64(std::numeric_limits<double>::infinity()) },
    { "I64_F64", true, U64(std::numeric_limits<int64_t>::min()), F64(-std::numeric_limits<double>::infinity()) },
    { "I64_F64", true, U64(0), F64(std::numeric_limits<double>::quiet_NaN()) },
    { "I64_F64", true, U64(9223372036854774784LL), F64(0x1.fffffffffffffp62) },
    { "I8_F32", true, U64(127), F32(127.75f) },
    { "I8_F32", true, U64(-128), F32(-128.75f) },
    { "I8_F64", true, U64(127), F64(127.75) },
    { "I8_F64", true, U64(-128), F64(-128.75) },
    { "I16_F32", true, U64(32767), F32(32767.75f) },
    { "I16_F32", true, U64(-32768), F32(-32768.75f) },
    { "I16_F64", true, U64(32767), F64(32767.75) },
    { "I16_F64", true, U64(-32768), F64(-32768.75) },
    { "I8_I32", false, U64(static_cast<uint64_t>(-128)), U64(32896) },
                                        { "I8_U32", false, U64(static_cast<uint64_t>(-128)), U64(32896) },
                                        { "I16_I32", false, U64(static_cast<uint64_t>(-32640)), U64(32896) },
                                        { "I16_U32", false, U64(static_cast<uint64_t>(-32640)), U64(32896) },
                                        { "I32_F32", true, U64(1), F32(1.0f) },
                                        { "I32_F64", true, U64(1), F64(1.0) },
                                        { "I32_I64", false, U64(1), U64(1) },
                                        { "I32_U64", false, U64(1), U64(1) },
                                        { "I64_F32", true, U64(1), F32(1.0f) },
                                        { "I64_F64", true, U64(1), F64(1.0) },
                                        { "I64_I32", false, U64(1), U32(1) },
                                        { "I64_U32", false, U64(1), U32(1) },
                                        { "U8_I32", false, U64(128), U32(32896) },
                                        { "U8_U32", false, U64(128), U32(32896) },
                                        { "U16_I32", false, U64(32896), U32(32896) },
                                        { "U16_U32", false, U64(32896), U32(32896) },
                                        { "U32_F32", true, U64(1), F32(1.0f) },
                                        { "U32_F64", true, U64(1), F64(1.0) },
                                        { "U32_U64", false, U64(1), U32(1) },
                                        { "U64_F32", true, U64(1), F32(1.0f) },
                                        { "U64_F64", true, U64(1), F64(1.0) } };

CastCase castToFloat32Cases[] = { { "F32_F64", true, F32(1.0f), F64(1.0) },
                                        { "F32_I32", false, F32(1.0f), U64(1) },
                                        { "F32_I64", false, F32(1.0f), U64(1) },
                                        { "F32_U32", false, F32(1.0f), U64(1) },
                                        { "F32_U64", false, F32(1.0f), U64(1) } };

static void castToInteger(Interpretation::Code code, CastCase castCase)
{
    auto ir1 = castCase.fromFP ? U64(0) : castCase.val;
    auto fr0 = castCase.fromFP ? castCase.val : F64(0);
    auto res = Interpret(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u64, castCase.expected.u64);
}

static void castToFloat32(Interpretation::Code code, CastCase castCase)
{
    auto ir1 = castCase.fromFP ? U64(0) : castCase.val;
    auto fr0 = castCase.fromFP ? castCase.val : F64(0);
    auto res = InterpretFPRes(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u32, castCase.expected.u32);
}

using CastTestFunction = void (*)(Interpretation::Code, CastCase);

struct CastTestParams {
    std::string name;
    int casesCount;
    CastCase* castCases;
    CastTestFunction testCast;
};

static std::ostream& operator<<(std::ostream& os, const CastTestParams& p) { return os << p.name; }

class CbcSpecializedCast : public ::testing::TestWithParam<CastTestParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedCast, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    CastTestParams params = GetParam();

    std::string                       path;
    Interpretation::ExecBytecodeInfo* info = nullptr;
    for (int n { 0 }; n < params.casesCount; ++n) {
        CastCase castCase = params.castCases[n];
        SCOPED_TRACE(castCase.name);
        auto casePath = "./generated-asm/cast/" + params.name + "/" + castCase.name + ".asm";
        if (casePath != path) {
            info = OpenAndRewrite("test_" + castCase.name, casePath);
            path = casePath;
        }
        params.testCast(info->code, castCase);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcSpecializedCast,
    ::testing::Values(
        CastTestParams { "to_integer",
                            static_cast<int>(std::size(castToIntegerCases)),
                            castToIntegerCases,
                            &castToInteger },
        CastTestParams {
            "to_float", static_cast<int>(std::size(castToFloat32Cases)), castToFloat32Cases, &castToFloat32 }
    )
);

template <typename T> using UnaryFunction = T (*)(T);

template <typename T> using BinaryFunction = T (*)(T, T);

template <typename T> T negFunc(T x) { return -x; }

template <typename T> T addFunc(T l, T r) { return l + r; }

template <typename T> T subFunc(T l, T r) { return l - r; }

template <typename T> T mulFunc(T l, T r) { return l * r; }

template <typename T> T divFunc(T l, T r) { return l / r; }

UnaryFunction<float> floatUnaryOps[] = { negFunc<float>, std::sqrt, std::abs,   std::sin,   std::cos,
                                         std::log,       std::ceil, std::floor, std::log10, std::log2,
                                         std::exp,       std::exp2, std::trunc };
constexpr size_t floatUnaryOpsCount  = std::size(floatUnaryOps);

BinaryFunction<float> floatBinaryOps[] = { addFunc<float>, subFunc<float>, mulFunc<float>, divFunc<float>, std::pow };
constexpr size_t floatBinaryOpsCount   = std::size(floatBinaryOps);

UnaryFunction<double> doubleUnaryOps[] = { negFunc<double>, std::sqrt, std::abs,   std::sin,   std::cos,
                                           std::log,        std::ceil, std::floor, std::log10, std::log2,
                                           std::exp,        std::exp2, std::trunc };
constexpr size_t doubleUnaryOpsCount   = std::size(doubleUnaryOps);

BinaryFunction<double> doubleBinaryOps[] = {
    addFunc<double>, subFunc<double>, mulFunc<double>, divFunc<double>, std::pow
};
constexpr size_t doubleBinaryOpsCount = std::size(doubleBinaryOps);

const char* unaryFunctionNames[] = { "NEG",   "SQRT",  "ABS",  "SIN", "COS",  "LOG",  "CEIL",
                                     "FLOOR", "LOG10", "LOG2", "EXP", "EXP2", "TRUNC" };

const char* binaryFunctionNames[] = { "ADD", "SUB", "MUL", "DIV", "POW" };

// Asm-level suites run a representative subset per op; the full op x value
// sweep over libm edge cases lives in the Emitter-level suites, which do not
// pay the external-assembler cost per case.
std::pair<float, float> floatValues[] = {
    { 4.2f, 7.3f },
    { 0.0f, -0.0f },
    { std::numeric_limits<float>::infinity(), 0.0f },
    { std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() },
    { -2.0f, 3.0f },
    { 1.0f, std::numeric_limits<float>::quiet_NaN() }
};

std::pair<double, double> doubleValues[] = {
    { 4.2, 7.3 },
    { 0.0, -0.0 },
    { std::numeric_limits<double>::infinity(), 0.0 },
    { std::numeric_limits<double>::max(), -std::numeric_limits<double>::max() },
    { -2.0, 3.0 },
    { 1.0, std::numeric_limits<double>::quiet_NaN() }
};

void testFPOps32(Interpretation::Value::Primitive res, float expVal)
{
    auto resVal = res.u32;
    if (std::isnan(resVal) && std::isnan(expVal)) {
        SUCCEED();
    } else {
        ASSERT_EQ(res.u32, F32(expVal).u32);
    }
}

void testFPOps64(Interpretation::Value::Primitive res, double expVal)
{
    auto resVal = res.u64;
    if (std::isnan(resVal) && std::isnan(expVal)) {
        SUCCEED();
    } else {
        ASSERT_EQ(res.u64, F64(expVal).u64);
    }
}

float unaryFloatValues[] = {
    0.0f, -0.0f, 0.5f, -2.5f, 1e10f, std::numeric_limits<float>::infinity(),
    -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()
};

double unaryDoubleValues[] = {
    0.0, -0.0, 0.5, -2.5, 1e10, std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
};

struct FPowiCase {
    double base;
    int64_t exponent;
};

FPowiCase fpowiCases[] = {
    { 2.0, 10 }, { 3.0, -2 }, { 5.0, 0 }, { 2.0, 63 }, { 0.5, -1 }, { 2.0, -63 }, { -2.0, 3 }, { -2.0, 4 },
    { 1.5, 100 }, { 0.0, 5 }, { 0.0, 0 }, { 2.0, INT64_MAX }, { 2.0, INT64_MIN }
};

class CbcFloatUnaryOp32 : public ::testing::TestWithParam<std::tuple<size_t, float>> {
    void SetUp() override { DoSetUp(); }
};

class CbcFloatUnaryOp64 : public ::testing::TestWithParam<std::tuple<size_t, double>> {
    void SetUp() override { DoSetUp(); }
};

class CbcFloatBinaryOp32 : public ::testing::TestWithParam<std::tuple<size_t, std::pair<float, float>>> {
    void SetUp() override { DoSetUp(); }
};

class CbcFloatBinaryOp64 : public ::testing::TestWithParam<std::tuple<size_t, std::pair<double, double>>> {
    void SetUp() override { DoSetUp(); }
};

class CbcFloatPowI32 : public ::testing::TestWithParam<FPowiCase> {
    void SetUp() override { DoSetUp(); }
};

class CbcFloatPowI64 : public ::testing::TestWithParam<FPowiCase> {
    void SetUp() override { DoSetUp(); }
};

std::string FloatUnaryOpName32(const ::testing::TestParamInfo<std::tuple<size_t, float>>& info)
{
    return std::string(unaryFunctionNames[std::get<0>(info.param)]) + "_v" + hex(F32(std::get<1>(info.param)).u32);
}

std::string DoubleUnaryOpName64(const ::testing::TestParamInfo<std::tuple<size_t, double>>& info)
{
    return std::string(unaryFunctionNames[std::get<0>(info.param)]) + "_v" + hex(F64(std::get<1>(info.param)).u64);
}

std::string FloatBinaryOpName32(const ::testing::TestParamInfo<std::tuple<size_t, std::pair<float, float>>>& info)
{
    return std::string(binaryFunctionNames[std::get<0>(info.param)]) + "_v" + hex(F32(std::get<1>(info.param).first).u32)
        + "_v" + hex(F32(std::get<1>(info.param).second).u32);
}

std::string DoubleBinaryOpName64(const ::testing::TestParamInfo<std::tuple<size_t, std::pair<double, double>>>& info)
{
    return std::string(binaryFunctionNames[std::get<0>(info.param)]) + "_v" + hex(F64(std::get<1>(info.param).first).u64)
        + "_v" + hex(F64(std::get<1>(info.param).second).u64);
}

std::string FPowiCaseName(const ::testing::TestParamInfo<FPowiCase>& info)
{
    int64_t exponent   = info.param.exponent;
    uint64_t magnitude = exponent < 0 ? 0ull - static_cast<uint64_t>(exponent) : static_cast<uint64_t>(exponent);
    std::string suffix = exponent < 0 ? "neg" + std::to_string(magnitude) : std::to_string(magnitude);
    return "v" + hex(F64(info.param.base).u64) + "_e" + suffix;
}

TEST_P(CbcFloatUnaryOp32, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto [opIdx, value] = GetParam();
    auto path           = std::string("./generated-asm/float-arith/32/") + unaryFunctionNames[opIdx] + ".asm";
    auto code           = OpenAndRewrite(std::string("test_") + unaryFunctionNames[opIdx], path)->code;
    auto res            = InterpretFPRes(code, F32(value), F32(1.f));
    auto expVal         = floatUnaryOps[opIdx](value);
    testFPOps32(res, expVal);
}

TEST_P(CbcFloatUnaryOp64, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto [opIdx, value] = GetParam();
    auto path           = std::string("./generated-asm/float-arith/64/") + unaryFunctionNames[opIdx] + ".asm";
    auto code           = OpenAndRewrite(std::string("test_") + unaryFunctionNames[opIdx], path)->code;
    auto res            = InterpretFPRes(code, F64(value), F64(1.f));
    auto expVal         = doubleUnaryOps[opIdx](value);
    testFPOps64(res, expVal);
}

TEST_P(CbcFloatBinaryOp32, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto [opIdx, value] = GetParam();
    auto [l, r]         = value;
    auto path           = std::string("./generated-asm/float-arith/32/") + binaryFunctionNames[opIdx] + ".asm";
    auto code           = OpenAndRewrite(std::string("test_") + binaryFunctionNames[opIdx], path)->code;
    auto res            = InterpretFPRes(code, F32(l), F32(r));
    auto expVal         = floatBinaryOps[opIdx](l, r);
    testFPOps32(res, expVal);
}

TEST_P(CbcFloatBinaryOp64, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto [opIdx, value] = GetParam();
    auto [l, r]         = value;
    auto path           = std::string("./generated-asm/float-arith/64/") + binaryFunctionNames[opIdx] + ".asm";
    auto code           = OpenAndRewrite(std::string("test_") + binaryFunctionNames[opIdx], path)->code;
    auto res            = InterpretFPRes(code, F64(l), F64(r));
    auto expVal         = doubleBinaryOps[opIdx](l, r);
    testFPOps64(res, expVal);
}

TEST_P(CbcFloatPowI32, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FPowiCase c = GetParam();
    auto code   = OpenAndRewrite("test_FPOWI", "./generated-asm/float-arith/32/FPOWI.asm")->code;
    auto base   = static_cast<float>(c.base);
    auto res    = InterpretFPRes(code, U64(c.exponent), U64(0), F32(0), F32(base));
    auto expVal = static_cast<float>(std::pow(static_cast<double>(base), static_cast<double>(c.exponent)));
    testFPOps32(res, expVal);
}

TEST_P(CbcFloatPowI64, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FPowiCase c = GetParam();
    auto code   = OpenAndRewrite("test_FPOWI", "./generated-asm/float-arith/64/FPOWI.asm")->code;
    auto res    = InterpretFPRes(code, U64(c.exponent), U64(0), F64(0), F64(c.base));
    auto expVal = std::pow(c.base, static_cast<double>(c.exponent));
    testFPOps64(res, expVal);
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcFloatUnaryOp32,
    ::testing::Combine(::testing::Range<size_t>(0, floatUnaryOpsCount), ::testing::ValuesIn(unaryFloatValues)),
    FloatUnaryOpName32
);

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcFloatUnaryOp64,
    ::testing::Combine(::testing::Range<size_t>(0, doubleUnaryOpsCount), ::testing::ValuesIn(unaryDoubleValues)),
    DoubleUnaryOpName64
);

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcFloatBinaryOp32,
    ::testing::Combine(::testing::Range<size_t>(0, floatBinaryOpsCount), ::testing::ValuesIn(floatValues)),
    FloatBinaryOpName32
);

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcFloatBinaryOp64,
    ::testing::Combine(::testing::Range<size_t>(0, doubleBinaryOpsCount), ::testing::ValuesIn(doubleValues)),
    DoubleBinaryOpName64
);

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcFloatPowI32, ::testing::ValuesIn(fpowiCases), FPowiCaseName);

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcFloatPowI64, ::testing::ValuesIn(fpowiCases), FPowiCaseName);

TEST_ASM(CbcTest, SimpleArithFloatMov32)
{
    {
        auto code = OpenAndRewrite("test_MOV", "./generated-asm/float-arith/32/MOV.asm")->code;
        auto res  = InterpretFPRes(code, F32(0), F32(2.0f));
        ASSERT_EQ(res.f32, 2.0f);
    }
    {
        auto code = OpenAndRewrite("test_F2I", "./generated-asm/float-arith/32/F2I.asm")->code;
        auto res  = Interpret(code, U32(0), U32(0), F32(2.5f), F32(0));
        ASSERT_EQ(res.u32, 0x40200000);
    }
    {
        auto code = OpenAndRewrite("test_I2F", "./generated-asm/float-arith/32/I2F.asm")->code;
        auto res  = InterpretFPRes(code, U32(0x40200000), U32(0), F32(0), F32(0));
        ASSERT_EQ(res.f32, 2.5f);
    }
}

TEST_ASM(CbcTest, SimpleArithFloatMov64)
{
    {
        auto code = OpenAndRewrite("test_MOV", "./generated-asm/float-arith/64/MOV.asm")->code;
        auto res  = InterpretFPRes(code, F64(0), F64(2.0));
        ASSERT_EQ(res.f64, 2.0);
    }
    {
        auto code = OpenAndRewrite("test_F2I", "./generated-asm/float-arith/64/F2I.asm")->code;
        auto res  = Interpret(code, U64(0), U64(0), F64(2.5), F64(0));
        ASSERT_EQ(res.u64, 0x4004000000000000);
    }
    {
        auto code = OpenAndRewrite("test_I2F", "./generated-asm/float-arith/64/I2F.asm")->code;
        auto res  = InterpretFPRes(code, U64(0x4004000000000000), U64(0), F64(0), F64(0));
        ASSERT_EQ(res.f64, 2.5);
    }
}

struct BFXCase {
    bool signExtend;
    uint32_t dstBits;
    uint32_t srcBits;
    Interpretation::Value::Primitive expected;
    Interpretation::Value::Primitive val;
};

BFXCase bfxSignedCases[] = {
    { true, 32, 32, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
    { true, 64, 32, U64(0xFFFFFFFFFF000000L), U64(0x000000FFFF000000L) },
    { true, 32, 64, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
    { true, 64, 64, U64(0xFFFFFFFFFF000000L), U64(0x000000FFFF000000L) },
};

BFXCase bfxZeroedCases[] = {
    { false, 32, 32, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
    { false, 64, 32, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
    { false, 32, 64, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
    { false, 64, 64, U64(0x00000000FF000000L), U64(0x000000FFFF000000L) },
};

struct BFXTestParams {
    std::string name;
    int casesCount;
    BFXCase* bfxCases;
};

static std::ostream& operator<<(std::ostream& os, const BFXTestParams& p) { return os << p.name; }

class BFX : public ::testing::TestWithParam<BFXTestParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(BFX, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    BFXTestParams params = GetParam();

    for (int n { 0 }; n < params.casesCount; ++n) {
        BFXCase bfxCase  = params.bfxCases[n];
        std::string sign = (bfxCase.signExtend ? "signed" : "zeroed");
        auto stem        = sign + "_" + std::to_string(bfxCase.dstBits) + "_" + std::to_string(bfxCase.srcBits);
        auto path        = "./generated-asm/bfx/" + stem + ".asm";
        auto code = OpenAndRewrite("test_" + stem, path)->code;
        auto res  = Interpret(code, bfxCase.val, U64(0), F64(0), F64(0));
        if (bfxCase.dstBits == 32)
            EXPECT_EQ(res.u32, bfxCase.expected.u32);
        if (bfxCase.dstBits == 64)
            EXPECT_EQ(res.u64, bfxCase.expected.u64);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    BFX,
    testing::Values(
        BFXTestParams { "signed", sizeof(bfxSignedCases) / sizeof(BFXCase), bfxSignedCases },
        BFXTestParams { "zeroed", sizeof(bfxZeroedCases) / sizeof(BFXCase), bfxZeroedCases }
    )
);

static int64_t SatSlsl(int64_t base, int64_t shift, int width)
{
    switch (width) {
        case 8:  base = static_cast<int8_t>(base); break;
        case 16: base = static_cast<int16_t>(base); break;
        case 32: base = static_cast<int32_t>(base); break;
        default: break;
    }
    if (shift < 0) {
        shift = 0;
    }
    if (shift > width - 1) {
        shift = width - 1;
    }
    unsigned __int128 ures = static_cast<unsigned __int128>(base) << shift;
    __int128 res           = static_cast<__int128>(ures);
    __int128 smin          = -(__int128(1) << (width - 1));
    __int128 smax          = (__int128(1) << (width - 1)) - 1;
    return static_cast<int64_t>(res < smin ? smin : (res > smax ? smax : res));
}

TEST_ASM(CbcTest, SatSlslNegativeBase)
{
    const int widths[] = { 8, 16, 32, 64 };
    const int shifts[] = { 1, 2, 7, 15, 31, 63 };

    for (auto width : widths) {
        auto boundary  = static_cast<int64_t>(-(__int128(1) << (width - 2)));
        int64_t bases[] = { -5, boundary, boundary - 1, std::numeric_limits<int64_t>::min() };
        for (auto shift : shifts) {
            if (shift > width) {
                continue;
            }
            auto stem = "slsl_" + std::to_string(width) + "_" + std::to_string(shift);
            auto code = OpenAndRewrite("test_" + stem, "./generated-asm/sat-arith/" + stem + ".asm")->code;
            for (auto base : bases) {
                auto res = Interpret(code, U64(base), U64(0));
                if (width == 64) {
                    ASSERT_EQ(res.u64, static_cast<uint64_t>(SatSlsl(base, shift, width)));
                } else {
                    ASSERT_EQ(res.u32, static_cast<uint32_t>(SatSlsl(base, shift, width)));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// arr.ic: array index check lowered through the rewriter
// ---------------------------------------------------------------------------

// Builds a fake array with the layout [TI*][u64 length][elements...] and
// returns its address as a primitive (iregs are a Primitive/Reference union,
// so the interpreter reads it back as a reference). Only the length is ever
// accessed: arr.len lowers to ld.64 at ObjectHeaderSize() and arr.ic compares
// registers, cf. the arr-ic suite in test/asm-generator/asm-generator.py.
TEST_ASM(CbcTest, ArrIndexCheck)
{
    TestTypeInfo ti { sizeof(uint64_t) * (2 + 4), 8 };
    uint64_t fakeArray[2 + 4] = { reinterpret_cast<uintptr_t>(&ti), 3 };
    auto arr                  = U64(reinterpret_cast<uintptr_t>(fakeArray));

    auto code = OpenAndRewrite("test_arr_ic", "./generated-asm/arr-ic/arr_ic.asm")->code;

    // In-bounds indices pass the check and flow into the rest of the method.
    ASSERT_EQ(Interpret(code, arr, U64(0)).u64, 0);
    ASSERT_EQ(Interpret(code, arr, U64(2)).u64, 2);
    // The first out-of-bounds index throws the implicit exception...
    ASSERT_EQ(InterpretThrows(code, arr, U64(3)), Interpretation::Type::IndexOutOfBoundsException);
    // ...and so does a negative index, which wraps to a huge unsigned value.
    ASSERT_EQ(
        InterpretThrows(code, arr, U64(static_cast<uint64_t>(-1))),
        Interpretation::Type::IndexOutOfBoundsException
    );
}

