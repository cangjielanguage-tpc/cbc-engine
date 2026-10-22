#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
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

static Engine::Engine& OpenGenerated(std::string_view fileName, std::unique_ptr<IO::RandomAccessFile> file)
{
    Engine::Loader loader;
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);
    auto& engine = loader.Build();
    BuiltEngines().push_back(&engine);
    return engine;
}

static const std::string generatedImageName = "generated.asm";

// Assembles an in-memory asm source (built on the fly) and opens it as an engine. The
// image is named after its auto-generated location; each test loads a single image.
static Engine::Engine& OpenSource(const std::string& source)
{
    return OpenGenerated(generatedImageName, OpenAsmText(generatedImageName, source));
}

static std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

static Interpretation::ExecBytecodeInfo* Rewrite(
    Engine::Engine& engine, std::string_view typeName, std::string_view methodName
)
{
    Engine::Session session(engine);
    auto fileName    = engine.Files().Front().name;
    auto methodId    = engine.FindMethod(session, fileName, typeName, methodName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh).info;
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
    auto methodId    = engine.FindMain(session, fileName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh).info;
}

static Interpretation::Value::Primitive Test(std::string name, std::string fileName)
{
    return Interpret(OpenAndRewrite(name, fileName)->code, U32(0), U32(10));
}

TEST_ASM(CbcTest, Simple)
{
    auto res = Interpret(OpenAndRewrite("simple", "simple.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, SimpleArith)
{
    auto res = Interpret(OpenAndRewrite("simple_arith", "simple_arith.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 36);
}

TEST_ASM(CbcTest, SimpleArithFloat)
{
    GTEST_SKIP() << "not supported";
    auto res = InterpretFPRes(OpenAndRewrite("simple_arith_float", "simple_arith_float.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.f64, 357);
}

TEST_ASM(CbcTest, SimpleArithImm)
{
    auto res = Interpret(OpenAndRewrite("simple_arith_imm", "simple_arith_imm.asm")->code, U32(0), U32(10));
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

    auto res = Interpret(OpenAndRewrite("fib-rec", "fib-rec.asm")->code, frame, U32(0), U32(0));
    ASSERT_EQ(res.u32, 13);
}

TEST_ASM(CbcTest, FibRecRegs)
{
    auto res = Interpret(OpenAndRewrite("fib-rec-regs", "fib-rec-regs.asm")->code, U32(0), U32(0));
    ASSERT_EQ(res.u32, 13);
}

TEST_ASM(CbcTest, DirectCall)
{
    auto res = Interpret(OpenAndRewrite("direct-call", "direct-call.asm")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, ArithSpecialized1)
{
    auto res = Interpret(OpenAndRewrite("arith_specialized1", "arith_specialized1.asm")->code, U64(10), U64(0));
    ASSERT_EQ(res.u64, 10 - 1);
}

TEST_ASM(CbcTest, ArithSpecialized2)
{
    auto res = Interpret(OpenAndRewrite("arith_specialized2", "arith_specialized2.asm")->code, U64(0), U64(0));
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
    auto mnemonic          = ToLower(params.name) + "i";

    std::ostringstream out;
    out << "@main_type \"default\"\n\n@type default\n";
    for (auto rhs : rhsValues) {
        auto imm = hex(rhs);
        out << "  @method test_" << imm << "()I64\n"
            << "    @code\n"
            << "      @live.prim IR1\n"
            << "      " << mnemonic << ".64 IR1, IR1, " << imm << "\n"
            << "      ret.64 IR1\n"
            << "    @end\n"
            << "  @end\n\n";
    }
    out << "@end\n";
    auto& engine = OpenSource(out.str());

    for (auto rhs : rhsValues) {
        auto code = Rewrite(engine, "default", "test_" + hex(rhs))->code;
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

struct ConvertCase {
    std::string name;
    bool fromFP;
    Interpretation::Value::Primitive expected;
    Interpretation::Value::Primitive val;
};

ConvertCase convertToIntegerCases[] = {
    { "U8_F32", true, U64(0), F32(0.0f) },
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
    { "U64_F64", true, U64(1), F64(1.0) }
};

ConvertCase convertToFloat32Cases[] = { { "F32_F64", true, F32(1.0f), F64(1.0) },
                                        { "F32_I32", false, F32(1.0f), U64(1) },
                                        { "F32_I64", false, F32(1.0f), U64(1) },
                                        { "F32_U32", false, F32(1.0f), U64(1) },
                                        { "F32_U64", false, F32(1.0f), U64(1) } };

ConvertCase convertToFloat64Cases[] = { { "F64_F32", true, F64(1.0), F32(1.0f) },
                                        { "F64_I32", false, F64(1.0), U64(1) },
                                        { "F64_I64", false, F64(1.0), U64(1) },
                                        { "F64_U32", false, F64(1.0), U64(1) },
                                        { "F64_U64", false, F64(1.0), U64(1) } };

static const std::vector<ConvertCase> convertToFloatCases = [] {
    std::vector<ConvertCase> cases(convertToFloat32Cases, convertToFloat32Cases + std::size(convertToFloat32Cases));
    cases.insert(cases.end(), convertToFloat64Cases, convertToFloat64Cases + std::size(convertToFloat64Cases));
    return cases;
}();

static void convertToInteger(Interpretation::Code code, ConvertCase convertCase)
{
    auto ir1 = convertCase.fromFP ? U64(0) : convertCase.val;
    auto fr0 = convertCase.fromFP ? convertCase.val : F64(0);
    auto res = Interpret(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u64, convertCase.expected.u64);
}

static void convertToFloat(Interpretation::Code code, ConvertCase convertCase)
{
    auto ir1 = convertCase.fromFP ? U64(0) : convertCase.val;
    auto fr0 = convertCase.fromFP ? convertCase.val : F64(0);
    auto res = InterpretFPRes(code, ir1, U64(0), fr0, F64(0));
    if (convertCase.name.rfind("F32", 0) == 0) {
        EXPECT_EQ(res.u32, convertCase.expected.u32);
    } else {
        EXPECT_EQ(res.u64, convertCase.expected.u64);
    }
}

using ConvertTestFunction = void (*)(Interpretation::Code, ConvertCase);

struct ConvertTestParams {
    std::string name;
    int casesCount;
    const ConvertCase* convertCases;
    ConvertTestFunction testConvert;
};

static std::ostream& operator<<(std::ostream& os, const ConvertTestParams& p) { return os << p.name; }

class CbcSpecializedConvert : public ::testing::TestWithParam<ConvertTestParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedConvert, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    ConvertTestParams params = GetParam();
    bool toFloat             = params.name == "to_float";

    std::vector<std::string> names;
    for (int n { 0 }; n < params.casesCount; ++n) {
        if (std::find(names.begin(), names.end(), params.convertCases[n].name) == names.end()) {
            names.push_back(params.convertCases[n].name);
        }
    }

    std::ostringstream out;
    out << "@main_type \"default\"\n\n@type default\n";
    for (const auto& name : names) {
        auto underscore = name.find('_');
        ASSERT(underscore != std::string::npos);
        auto t0    = name.substr(0, underscore);
        auto t1    = name.substr(underscore + 1);
        auto fpSrc = t1 == "F32" || t1 == "F64";
        auto op    = std::string(fpSrc ? "f2" : "i2") + (toFloat ? 'f' : 'i');
        auto regs  = fpSrc ? (toFloat ? "FR0, FR0" : "FR0, IR1") : (toFloat ? "IR1, FR0" : "IR1, IR1");
        out << "  @method test_" << name << "()I64\n"
            << "    @code\n";
        if (toFloat || !fpSrc) {
            out << "      @live.prim IR1\n";
        }
        out << "      " << op << " " << t1 << ", " << t0 << ", " << regs << "\n"
            << "      " << (toFloat ? "fret.64 FR0" : "ret.64 IR1") << "\n"
            << "    @end\n"
            << "  @end\n\n";
    }
    out << "@end\n";
    auto& engine = OpenSource(out.str());

    for (int n { 0 }; n < params.casesCount; ++n) {
        ConvertCase convertCase = params.convertCases[n];
        SCOPED_TRACE(convertCase.name);
        auto code = Rewrite(engine, "default", "test_" + convertCase.name)->code;
        params.testConvert(code, convertCase);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcSpecializedConvert,
    ::testing::Values(
        ConvertTestParams { "to_integer",
                            static_cast<int>(std::size(convertToIntegerCases)),
                            convertToIntegerCases,
                            &convertToInteger },
        ConvertTestParams {
            "to_float", static_cast<int>(convertToFloatCases.size()), convertToFloatCases.data(), &convertToFloat }
    )
);

template <typename T> using UnaryFunction = T (*)(T);

template <typename T> using BinaryFunction = T (*)(T, T);

template <typename T> T negFunc(T x) { return -x; }

template <typename T> T addFunc(T l, T r) { return l + r; }

template <typename T> T subFunc(T l, T r) { return l - r; }

template <typename T> T mulFunc(T l, T r) { return l * r; }

template <typename T> T divFunc(T l, T r) { return l / r; }

const char* unaryFunctionNames[] = { "NEG", "SQRT", "ABS" };

const char* binaryFunctionNames[] = { "ADD", "SUB", "MUL", "DIV" };

std::pair<float, float> floatValues[] = {
    { 4.2f, 7.3f },
    { 0.0f, -0.0f },
    { -0.0f, -0.0f },
    { std::numeric_limits<float>::quiet_NaN(), 12.34f },
    { std::numeric_limits<float>::infinity(), 12.34f },
    { std::numeric_limits<float>::infinity(), 0.0f },
    { -std::numeric_limits<float>::infinity(), 12.34f },
    { std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() },
    { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() },
    { std::numeric_limits<float>::max(), 2.0f },
    { std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() },
    { 16777216.0f, 1.0f },
    { 16777216.0f, 2.0f }
};
constexpr size_t floatValuesCount = std::size(floatValues);

std::pair<double, double> doubleValues[] = {
    { 4.2, 7.3 },
    { 0.0, -0.0 },
    { -0.0, -0.0 },
    { std::numeric_limits<double>::quiet_NaN(), 12.34 },
    { std::numeric_limits<double>::infinity(), 12.34 },
    { std::numeric_limits<double>::infinity(), 0.0 },
    { -std::numeric_limits<double>::infinity(), 12.34 },
    { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() },
    { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() },
    { std::numeric_limits<double>::max(), 2.0 },
    { std::numeric_limits<double>::max(), -std::numeric_limits<double>::max() },
    { 9007199254740992.0, 1.0 },
    { 9007199254740992.0, 2.0 }
};
constexpr size_t doubleValuesCount = std::size(doubleValues);

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

class CbcFloatOps : public ::testing::TestWithParam<int> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcFloatOps, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto width = GetParam();

    std::ostringstream out;
    out << "@main_type \"default\"\n\n@type default\n";
    std::string sep;
    for (auto name : { "NEG", "SQRT", "ABS" }) {
        out << sep << "  @method test_" << name << "()F" << width << "\n"
            << "    @code\n"
            << "      f" << ToLower(name) << "." << width << " FR0, FR0\n"
            << "      fret." << width << " FR0\n"
            << "    @end\n"
            << "  @end\n";
        sep = "\n";
    }
    for (auto name : { "ADD", "SUB", "MUL", "DIV" }) {
        out << sep << "  @method test_" << name << "()F" << width << "\n"
            << "    @code\n"
            << "      f" << ToLower(name) << "." << width << " FR0, FR0, FR1\n"
            << "      fret." << width << " FR0\n"
            << "    @end\n"
            << "  @end\n";
        sep = "\n";
    }
    out << "@end\n";
    auto& engine = OpenSource(out.str());

    auto runOps = [&](auto tag) {
        using T                         = decltype(tag);
        UnaryFunction<T> unaryFuncs[]   = { negFunc<T>, std::sqrt, std::abs };
        BinaryFunction<T> binaryFuncs[] = { addFunc<T>, subFunc<T>, mulFunc<T>, divFunc<T> };
        std::pair<T, T>* values;
        size_t valuesCount;
        if constexpr (sizeof(T) == 4) {
            values      = floatValues;
            valuesCount = floatValuesCount;
        } else {
            values      = doubleValues;
            valuesCount = doubleValuesCount;
        }
        auto pack = [](T v) {
            if constexpr (sizeof(T) == 4)
                return F32(v);
            else
                return F64(v);
        };
        auto check = [&](auto res, T exp) {
            if constexpr (sizeof(T) == 4) {
                testFPOps32(res, exp);
            } else {
                testFPOps64(res, exp);
            }
        };
        for (size_t i { 0 }; i < valuesCount; ++i) {
            auto [l, r] = values[i];
            for (size_t j { 0 }; j < std::size(unaryFuncs); ++j) {
                auto res = InterpretFPRes(
                    Rewrite(engine, "default", std::string("test_") + unaryFunctionNames[j])->code, pack(l), pack(T(1))
                );
                check(res, unaryFuncs[j](l));
            }
            for (size_t j { 0 }; j < std::size(binaryFuncs); ++j) {
                auto res = InterpretFPRes(
                    Rewrite(engine, "default", std::string("test_") + binaryFunctionNames[j])->code, pack(l), pack(r)
                );
                check(res, binaryFuncs[j](l, r));
            }
        }
    };
    if (width == 32) {
        runOps(float {});
    } else {
        runOps(double {});
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest, CbcFloatOps, ::testing::Values(32, 64), [](const ::testing::TestParamInfo<int>& info) {
        return std::to_string(info.param);
    }
);

class CbcFloatMov : public ::testing::TestWithParam<int> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcFloatMov, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    auto width = GetParam();

    std::ostringstream out;
    out << "@main_type \"default\"\n\n@type default\n"
        << "  @method test_MOV()F" << width << "\n"
        << "    @code\n"
        << "      fmov." << width << " FR0, FR1\n"
        << "      fret." << width << " FR0\n"
        << "    @end\n"
        << "  @end\n\n"
        << "  @method test_I2F()F" << width << "\n"
        << "    @code\n"
        << "      @live.prim IR1\n"
        << "      movi2f." << width << " FR0, IR1\n"
        << "      fret." << width << " FR0\n"
        << "    @end\n"
        << "  @end\n\n"
        << "  @method test_F2I()I" << width << "\n"
        << "    @code\n"
        << "      movf2i." << width << " IR1, FR0\n"
        << "      ret." << width << " IR1\n"
        << "    @end\n"
        << "  @end\n"
        << "@end\n";
    auto& engine = OpenSource(out.str());

    auto runMov = [&](auto tag) {
        using T    = decltype(tag);
        auto packF = [](T v) {
            if constexpr (sizeof(T) == 4)
                return F32(v);
            else
                return F64(v);
        };
        auto packI = [](uint64_t v) {
            if constexpr (sizeof(T) == 4)
                return U32(uint32_t(v));
            else
                return U64(v);
        };
        {
            auto res = InterpretFPRes(Rewrite(engine, "default", "test_MOV")->code, packF(T(0)), packF(T(2)));
            if constexpr (sizeof(T) == 4) {
                ASSERT_EQ(res.f32, T(2));
            } else {
                ASSERT_EQ(res.f64, T(2));
            }
        }
        {
            auto res =
                Interpret(Rewrite(engine, "default", "test_F2I")->code, packI(0), packI(0), packF(T(2.5)), packF(T(0)));
            if constexpr (sizeof(T) == 4) {
                ASSERT_EQ(res.u32, packF(T(2.5)).u32);
            } else {
                ASSERT_EQ(res.u64, packF(T(2.5)).u64);
            }
        }
        {
            auto res = InterpretFPRes(
                Rewrite(engine, "default", "test_I2F")->code,
                packI(sizeof(T) == 4 ? packF(T(2.5)).u32 : packF(T(2.5)).u64),
                packI(0),
                packF(T(0)),
                packF(T(0))
            );
            if constexpr (sizeof(T) == 4) {
                ASSERT_EQ(res.f32, T(2.5));
            } else {
                ASSERT_EQ(res.f64, T(2.5));
            }
        }
    };
    if (width == 32) {
        runMov(float {});
    } else {
        runMov(double {});
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest, CbcFloatMov, ::testing::Values(32, 64), [](const ::testing::TestParamInfo<int>& info) {
        return std::to_string(info.param);
    }
);

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
        auto path        = "./bfx/simple_bfx_" + sign + "_" + std::to_string(bfxCase.dstBits) + "_" +
                    std::to_string(bfxCase.srcBits) + ".asm";
        auto code = OpenAndRewrite("bfx", path)->code;
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
