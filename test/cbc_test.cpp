#include <gtest/gtest.h>

#include <cstdint>
#include <iterator>
#include <sstream>

#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "engine/image/io/byte_array_random_access_file.h"
#include "engine/image/reader.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"

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
    char* data        = new char[view.size() + 1];
    data[view.size()] = 0;
    view.copy(data, view.size());

    return std::make_unique<IO::ByteArrayRandomAccessFile>(data, view.size());
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
    auto str     = Image::Reader::Read(session, Image::FileId(0), strOffs);

    ASSERT_EQ(str, "abc");
}

static Engine::Engine& Open(std::string_view fileName)
{
    Engine::Loader loader;
    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);
    return loader.Build();
}

static Interpretation::ExecBytecodeInfo* Rewrite(
    Engine::Engine& engine, std::string_view fileName, std::string_view typeName, std::string_view methodName
)
{
    Engine::Session session(engine);
    auto methodId    = engine.FindMethod(session, fileName, typeName, methodName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh);
}

static Interpretation::ExecBytecodeInfo* OpenAndRewrite(std::string name, std::string_view fileName)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto methodId    = engine.FindMain(session, fileName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh);
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
    uint64_t frameSlots[1];
    auto frameStart = reinterpret_cast<uintptr_t>(&frameSlots);
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
    return static_cast<int64_t>(left >> (rhs & 0x3f));
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
    auto path              = "./simple_arith_specialized/simple_arith_specialized_" + params.name + "_bulk.asm";
    auto& engine           = Open(path);

    for (auto rhs : rhsValues) {
        auto code = Rewrite(engine, path, "default", "test_" + hex(rhs))->code;
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
    { "I8_I32", false, U64(-128), U64(32896) },    { "I8_U32", false, U64(-128), U64(32896) },
    { "I16_I32", false, U64(-32640), U64(32896) }, { "I16_U32", false, U64(-32640), U64(32896) },
    { "I32_F32", true, U64(1), F32(1.0f) },        { "I32_F64", true, U64(1), F64(1.0) },
    { "I32_I64", false, U64(1), U64(1) },          { "I32_U64", false, U64(1), U64(1) },
    { "I64_F32", true, U64(1), F32(1.0f) },        { "I64_F64", true, U64(1), F64(1.0) },
    { "I64_I32", false, U64(1), U32(1) },          { "I64_U32", false, U64(1), U32(1) },
    { "U8_I32", false, U64(128), U32(32896) },     { "U8_U32", false, U64(128), U32(32896) },
    { "U16_I32", false, U64(32896), U32(32896) },  { "U16_U32", false, U64(32896), U32(32896) },
    { "U32_F32", true, U64(1), F32(1.0f) },        { "U32_F64", true, U64(1), F64(1.0) },
    { "U32_U64", false, U64(1), U32(1) },          { "U64_F32", true, U64(1), F32(1.0f) },
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

static void convertToInteger(Interpretation::Code code, ConvertCase convertCase)
{
    auto ir1 = convertCase.fromFP ? U64(0) : convertCase.val;
    auto fr0 = convertCase.fromFP ? convertCase.val : F64(0);
    auto res = Interpret(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u64, convertCase.expected.u64);
}

static void convertToFloat32(Interpretation::Code code, ConvertCase convertCase)
{
    auto ir1 = convertCase.fromFP ? U64(0) : convertCase.val;
    auto fr0 = convertCase.fromFP ? convertCase.val : F64(0);
    auto res = InterpretFPRes(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u32, convertCase.expected.u32);
}

static void convertToFloat64(Interpretation::Code code, ConvertCase convertCase)
{
    auto ir1 = convertCase.fromFP ? U64(0) : convertCase.val;
    auto fr0 = convertCase.fromFP ? convertCase.val : F64(0);
    auto res = InterpretFPRes(code, ir1, U64(0), fr0, F64(0));
    EXPECT_EQ(res.u64, convertCase.expected.u64);
}

using ConvertTestFunction = void (*)(Interpretation::Code, ConvertCase);

struct ConvertTestParams {
    std::string name;
    int casesCount;
    ConvertCase* convertCases;
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
    auto path                = "./simple_convert/simple_convert_" + params.name + ".asm";
    auto& engine             = Open(path);

    for (int n { 0 }; n < params.casesCount; ++n) {
        ConvertCase convertCase = params.convertCases[n];
        auto code               = Rewrite(engine, path, "default", "test_" + convertCase.name)->code;
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
            "to_float", static_cast<int>(std::size(convertToFloat32Cases)), convertToFloat32Cases, &convertToFloat32 }
        //ConvertTestParams {
        //    "to_float", static_cast<int>(std::size(convertToFloat64Cases)), convertToFloat64Cases, &convertToFloat64 }
    )
);

template <typename T> using UnaryFunction = T (*)(T);

template <typename T> using BinaryFunction = T (*)(T, T);

template <typename T> T negFunc(T x) { return -x; }

template <typename T> T addFunc(T l, T r) { return l + r; }

template <typename T> T subFunc(T l, T r) { return l - r; }

template <typename T> T mulFunc(T l, T r) { return l * r; }

template <typename T> T divFunc(T l, T r) { return l / r; }

UnaryFunction<float> floatUnaryOps[] = { negFunc<float>, std::sqrt, std::abs };
constexpr size_t floatUnaryOpsCount  = std::size(floatUnaryOps);

BinaryFunction<float> floatBinaryOps[] = { addFunc<float>, subFunc<float>, mulFunc<float>, divFunc<float> };
constexpr size_t floatBinaryOpsCount   = std::size(floatBinaryOps);

UnaryFunction<double> doubleUnaryOps[] = { negFunc<double>, std::sqrt, std::abs };
constexpr size_t doubleUnaryOpsCount   = std::size(doubleUnaryOps);

BinaryFunction<double> doubleBinaryOps[] = { addFunc<double>, subFunc<double>, mulFunc<double>, divFunc<double> };
constexpr size_t doubleBinaryOpsCount    = std::size(doubleBinaryOps);

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

template <typename T> struct FPOpsTestParams {
    std::string name;
    size_t valuesCount;
    std::pair<T, T>* values;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const FPOpsTestParams<T>& params)
{
    return os << params.name;
}

template <typename T>
static std::string FPOpsTestParamsName(const ::testing::TestParamInfo<FPOpsTestParams<T>>& info)
{
    return info.param.name;
}

class CbcSpecializedFloatOps : public ::testing::TestWithParam<FPOpsTestParams<float>> {
    void SetUp() override { DoSetUp(); }
};

class CbcSpecializedDoubleOps : public ::testing::TestWithParam<FPOpsTestParams<double>> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedFloatOps, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FPOpsTestParams<float> params = GetParam();
    auto path                     = "./simple_arith_float/simple_arith_float" + params.name + ".asm";
    auto& engine                  = Open(path);
    for (size_t i { 0 }; i < params.valuesCount; ++i) {
        auto [l, r] = params.values[i];
        for (size_t j { 0 }; j < floatUnaryOpsCount; ++j) {
            UnaryFunction<float> unaryFunc = floatUnaryOps[j];
            const char* unaryFuncName      = unaryFunctionNames[j];
            auto res                       = InterpretFPRes(
                Rewrite(engine, path, "default", std::string("test_") + unaryFuncName)->code, F32(l), F32(1.f)
            );
            auto expVal = unaryFunc(l);
            testFPOps32(res, expVal);
        }
        for (size_t j { 0 }; j < floatBinaryOpsCount; ++j) {
            BinaryFunction<float> binaryFunc = floatBinaryOps[j];
            const char* binaryFuncName       = binaryFunctionNames[j];
            auto res                         = InterpretFPRes(
                Rewrite(engine, path, "default", std::string("test_") + binaryFuncName)->code, F32(l), F32(r)
            );
            auto expVal = binaryFunc(l, r);
            testFPOps32(res, expVal);
        }
    }
}

TEST_P(CbcSpecializedDoubleOps, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FPOpsTestParams<double> params = GetParam();
    auto path                      = "./simple_arith_float/simple_arith_float" + params.name + ".asm";
    auto& engine                   = Open(path);
    for (size_t i { 0 }; i < params.valuesCount; ++i) {
        auto [l, r] = params.values[i];
        for (size_t j { 0 }; j < floatUnaryOpsCount; ++j) {
            UnaryFunction<double> unaryFunc = doubleUnaryOps[j];
            const char* unaryFuncName       = unaryFunctionNames[j];
            auto res                        = InterpretFPRes(
                Rewrite(engine, path, "default", std::string("test_") + unaryFuncName)->code, F64(l), F64(1.f)
            );
            auto expVal = unaryFunc(l);
            testFPOps64(res, expVal);
        }
        for (size_t j { 0 }; j < floatBinaryOpsCount; ++j) {
            BinaryFunction<double> binaryFunc = doubleBinaryOps[j];
            const char* binaryFuncName        = binaryFunctionNames[j];
            auto res                          = InterpretFPRes(
                Rewrite(engine, path, "default", std::string("test_") + binaryFuncName)->code, F64(l), F64(r)
            );
            auto expVal = binaryFunc(l, r);
            testFPOps64(res, expVal);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest, CbcSpecializedFloatOps, ::testing::Values(FPOpsTestParams<float> { "32", floatValuesCount, floatValues }),
    FPOpsTestParamsName<float>
);

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    CbcSpecializedDoubleOps,
    ::testing::Values(FPOpsTestParams<double> { "64", doubleValuesCount, doubleValues }),
    FPOpsTestParamsName<double>
);

TEST_ASM(CbcTest, SimpleArithFloatMov32)
{
    auto path    = "./simple_arith_float/simple_arith_float32_mov.asm";
    auto& engine = Open(path);

    {
        auto res = InterpretFPRes(Rewrite(engine, path, "default", "test_MOV")->code, F32(0), F32(2.0f));
        ASSERT_EQ(res.f32, 2.0f);
    }
    {
        auto res = Interpret(Rewrite(engine, path, "default", "test_F2I")->code, U32(0), U32(0), F32(2.5f), F32(0));
        ASSERT_EQ(res.u32, 0x40200000);
    }
    {
        auto res =
            InterpretFPRes(Rewrite(engine, path, "default", "test_I2F")->code, U32(0x40200000), U32(0), F32(0), F32(0));
        ASSERT_EQ(res.f32, 2.5f);
    }
}

TEST_ASM(CbcTest, SimpleArithFloatMov64)
{
    auto path    = "./simple_arith_float/simple_arith_float64_mov.asm";
    auto& engine = Open(path);

    {
        auto res = InterpretFPRes(Rewrite(engine, path, "default", "test_MOV")->code, F64(0), F64(2.0));
        ASSERT_EQ(res.f64, 2.0);
    }
    {
        auto res = Interpret(Rewrite(engine, path, "default", "test_F2I")->code, U64(0), U64(0), F64(2.5), F64(0));
        ASSERT_EQ(res.u64, 0x4004000000000000);
    }
    {
        auto res = InterpretFPRes(
            Rewrite(engine, path, "default", "test_I2F")->code, U64(0x4004000000000000), U64(0), F64(0), F64(0)
        );
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
        BFXCase bfxCase = params.bfxCases[n];
        std::string sign = (bfxCase.signExtend ? "signed" : "zeroed");
        auto path = "./bfx/simple_bfx_" + sign +
            "_" + std::to_string(bfxCase.dstBits) +
            "_" + std::to_string(bfxCase.srcBits) + ".asm";
        auto code = OpenAndRewrite("bfx", path)->code;
        auto res  = Interpret(code, bfxCase.val, U64(0), F64(0), F64(0));
        if (bfxCase.dstBits == 32) EXPECT_EQ(res.u32, bfxCase.expected.u32);
        if (bfxCase.dstBits == 64) EXPECT_EQ(res.u64, bfxCase.expected.u64);
    }
}

INSTANTIATE_TEST_SUITE_P(
    CbcTest,
    BFX,
    testing::Values(
        BFXTestParams {
            "signed", sizeof(bfxSignedCases) / sizeof(BFXCase), bfxSignedCases },
        BFXTestParams {
            "zeroed", sizeof(bfxZeroedCases) / sizeof(BFXCase), bfxZeroedCases }
    )
);
