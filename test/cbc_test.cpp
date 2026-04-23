#include <cstdint>
#include <gtest/gtest.h>

#include "cbc/formater_rt.h"
#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "engine/symlevel/io/byte_array_random_access_file.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "interpreter/code.h"
#include "interpreter/function_handle.h"

#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class CbcTest : public testing::Test {
    void SetUp() override
    {
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
    auto strOffs = Symlevel::Offset<Symlevel::String>(offs);
    auto str     = Symlevel::Reader::Read(session, IO::FileId(0), strOffs);

    ASSERT_EQ(str, "abc");
}

static Engine::Engine& OpenFile(std::string_view fileName)
{
    Engine::Loader loader;
    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);
    return loader.Build();
}

static Interpretation::ExecBytecodeInfo* RewriteMethod(
    Engine::Engine& engine, std::string_view fileName, std::string_view typeName, std::string_view methodName
)
{
    Engine::Session session(engine);
    auto methodId    = engine.FindMethod(session, fileName, typeName, methodName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh);
}

static Interpretation::ExecBytecodeInfo* OpenAndRewrite(
    std::string name, std::string_view fileName, std::string_view typeName, std::string_view methodName
)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto methodId    = engine.FindMethod(session, fileName, typeName, methodName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, methodId.value()));
    return fuhManager.Prepare(session, fuh);
}

static Interpretation::Value::Primitive Test(std::string name, std::string fileName)
{
    return Interpret(OpenAndRewrite(name, fileName, "default", "main")->code, U32(0), U32(10));
}

TEST_ASM(CbcTest, Simple)
{
    auto res = Interpret(OpenAndRewrite("simple", "simple.asm", "default", "main")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, SimpleArith)
{
    auto res = Interpret(OpenAndRewrite("simple_arith", "simple_arith.asm", "default", "main")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 36);
}

TEST_ASM(CbcTest, SimpleArithImm)
{
    auto res =
        Interpret(OpenAndRewrite("simple_arith_imm", "simple_arith_imm.asm", "default", "main")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 1);
}

TEST_ASM(CbcTest, DirectCall)
{
    auto res = Interpret(OpenAndRewrite("direct-call", "direct-call.asm", "default", "main")->code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, ArithSpecialized1)
{
    auto res = Interpret(
        OpenAndRewrite("arith_specialized1", "arith_specialized1.asm", "default", "main")->code, U64(10), U64(0)
    );
    ASSERT_EQ(res.u64, 10 - 1);
}

TEST_ASM(CbcTest, ArithSpecialized2)
{
    auto res = Interpret(
        OpenAndRewrite("arith_specialized2", "arith_specialized2.asm", "default", "main")->code, U64(0), U64(0)
    );
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

#define SIMPLE_ARITH_OPC(X)                                                                                            \
    X(Add)                                                                                                             \
    X(Sub)                                                                                                             \
    X(Mul)                                                                                                             \
    X(And)                                                                                                             \
    X(Or)                                                                                                              \
    X(Xor)                                                                                                             \
    X(UDiv)                                                                                                            \
    X(Div)                                                                                                             \
    X(Rem)                                                                                                             \
    X(URem)                                                                                                            \
    X(LSL)                                                                                                             \
    X(LSR)                                                                                                             \
    X(ASR)

#define SIMPLE_ARITH_VALUES(X, opc)                                                                                    \
    X(opc, 0x1)                                                                                                        \
    X(opc, 0x10)                                                                                                       \
    X(opc, 0x100)                                                                                                      \
    X(opc, 0x1020)                                                                                                     \
    X(opc, 0x10000)                                                                                                    \
    X(opc, 0x100200)                                                                                                   \
    X(opc, 0x1000020)                                                                                                  \
    X(opc, 0x7000000000000000)                                                                                         \
    X(opc, 0x7000000010000001)                                                                                         \
    X(opc, 0xf000100000000001)                                                                                         \
    X(opc, 0xf000000100000001)

#define SIMPLE_ARITH_SPECIALIZED_VALUE(opc, value)                                                                     \
    {                                                                                                                  \
        auto code = RewriteMethod(engine, path, "default", "test_" #value)->code;                                      \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 1, value);                                                                  \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 20, value);                                                                 \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 301, value);                                                                \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 402, value);                                                                \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 0x3311, value);                                                             \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 0x7222222222222222, value);                                                 \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 0xf111111111111111, value);                                                 \
        SIMPLE_ARITH_SPECIALIZED_CASE(opc, 0xffffffffffffffff, value);                                                 \
    }

#define SIMPLE_ARITH_SPECIALIZED_CASE(opc, left, right)                                                                \
    {                                                                                                                  \
        auto res = Interpret(code, U64(left), U64(0));                                                                 \
        EXPECT_EQ(res.u64, (opc)((left), (right)));                                                                    \
    }

#define SIMPLE_ARITH_SPECIALIZED(opc)                                                                                  \
    TEST_ASM(CbcTest, SimpleArithSpecialized##opc)                                                                     \
    {                                                                                                                  \
        auto path    = "./simple_arith_specialized/simple_arith_specialized_" #opc "_bulk.asm";                        \
        auto& engine = OpenFile(path);                                                                                 \
        SIMPLE_ARITH_VALUES(SIMPLE_ARITH_SPECIALIZED_VALUE, opc)                                                       \
    }

SIMPLE_ARITH_OPC(SIMPLE_ARITH_SPECIALIZED)

#undef SIMPLE_ARITH_OPC
#undef SIMPLE_ARITH_VALUES
#undef SIMPLE_ARITH_SPECIALIZED_VALUE
#undef SIMPLE_ARITH_SPECIALIZED_CASE
#undef SIMPLE_ARITH_SPECIALIZED

#define SIMPLE_CONVERT_TO_INTEGER_CASES(X)                                                                             \
    X(I8_I32, false, false, U64(-128), U64(32896))                                                                     \
    X(I8_U32, false, false, U64(-128), U64(32896))                                                                     \
    X(I16_I32, false, false, U64(-32640), U64(32896))                                                                  \
    X(I16_U32, false, false, U64(-32640), U64(32896))                                                                  \
    X(I32_F32, false, true, U64(1), F32(1.0f))                                                                         \
    X(I32_F64, false, true, U64(1), F64(1.0))                                                                          \
    X(I32_I64, false, false, U64(1), U64(1))                                                                           \
    X(I32_U64, false, false, U64(1), U64(1))                                                                           \
    X(I64_F32, false, true, U64(1), F32(1.0f))                                                                         \
    X(I64_F64, false, true, U64(1), F64(1.0))                                                                          \
    X(I64_I32, false, false, U64(1), U32(1))                                                                           \
    X(I64_U32, false, false, U64(1), U32(1))                                                                           \
    X(U8_I32, false, false, U64(128), U32(32896))                                                                      \
    X(U8_U32, false, false, U64(128), U32(32896))                                                                      \
    X(U16_I32, false, false, U64(32896), U32(32896))                                                                   \
    X(U16_U32, false, false, U64(32896), U32(32896))                                                                   \
    X(U32_F32, false, true, U64(1), F32(1.0f))                                                                         \
    X(U32_F64, false, true, U64(1), F64(1.0))                                                                          \
    X(U32_U64, false, false, U64(1), U32(1))                                                                           \
    X(U64_F32, false, true, U64(1), F32(1.0f))                                                                         \
    X(U64_F64, false, true, U64(1), F64(1.0))

#define SIMPLE_CONVERT_TO_FLOAT_CASES(X)                                                                               \
    X(F32_F64, true, true, F32(1.0f), F64(1.0))                                                                        \
    X(F32_I32, true, false, F32(1.0f), U64(1))                                                                         \
    X(F32_I64, true, false, F32(1.0f), U64(1))                                                                         \
    X(F32_U32, true, false, F32(1.0f), U64(1))                                                                         \
    X(F32_U64, true, false, F32(1.0f), U64(1))                                                                         \
    X(F64_F32, true, true, F64(1.0), F32(1.0f))                                                                        \
    X(F64_I32, true, false, F64(1.0), U64(1))                                                                          \
    X(F64_I64, true, false, F64(1.0), U64(1))                                                                          \
    X(F64_U32, true, false, F64(1.0), U64(1))                                                                          \
    X(F64_U64, true, false, F64(1.0), U64(1))

#define SIMPLE_CONVERT_CASES(X)                                                                                        \
    X(to_integer, SIMPLE_CONVERT_TO_INTEGER_CASES)                                                                     \
    X(to_float, SIMPLE_CONVERT_TO_FLOAT_CASES)

#define SIMPLE_CONVERT_CASES_TEST(opc, toFP, fromFP, expected, val)                                                    \
    {                                                                                                                  \
        auto code = RewriteMethod(engine, path, "default", "test_" #opc)->code;                                        \
        auto ir1  = fromFP ? U64(0) : val;                                                                             \
        auto fr0  = fromFP ? val : F64(0);                                                                             \
        auto res  = toFP ? InterpretFPRes(code, ir1, U64(0), fr0, F64(0)) : Interpret(code, ir1, U64(0), fr0, F64(0)); \
        EXPECT_EQ(res.u64, expected.u64);                                                                              \
    }

#define SIMPLE_CONVERT_TEST(toType, CASES)                                                                             \
    TEST_ASM(CbcTest, SimpleConvert_##toType)                                                                          \
    {                                                                                                                  \
        auto path    = "./simple_convert/simple_convert_" #toType ".asm";                                              \
        auto& engine = OpenFile(path);                                                                                 \
        CASES(SIMPLE_CONVERT_CASES_TEST)                                                                               \
    }

SIMPLE_CONVERT_CASES(SIMPLE_CONVERT_TEST)

#undef SIMPLE_CONVERT_TO_INTEGER_CASES
#undef SIMPLE_CONVERT_TO_FLOAT_CASES
#undef SIMPLE_CONVERT_CASES
#undef SIMPLE_CONVERT_CASES_TEST
#undef SIMPLE_CONVERT_TEST

#define SIMPLE_ARITH_FLOAT_BINARY_CASES_32(X, opc, op, type, F)                                                        \
    X(opc, op, type, F, 4.2f, 7.3f)                                                                                    \
    X(opc, op, type, F, 0.0f, -0.0f)                                                                                   \
    X(opc, op, type, F, -0.0f, -0.0f)                                                                                  \
    X(opc, op, type, F, std::numeric_limits<float>::quiet_NaN(), 12.34f)                                               \
    X(opc, op, type, F, std::numeric_limits<float>::infinity(), 12.34f)                                                \
    X(opc, op, type, F, std::numeric_limits<float>::infinity(), 0.0f)                                                  \
    X(opc, op, type, F, -std::numeric_limits<float>::infinity(), 12.34f)                                               \
    X(opc, op, type, F, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())               \
    X(opc, op, type, F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN())               \
    X(opc, op, type, F, std::numeric_limits<float>::max(), 2.0f)                                                       \
    X(opc, op, type, F, std::numeric_limits<float>::max(), -std::numeric_limits<float>::max())                         \
    X(opc, op, type, F, 16777216.0f, 1.0f)                                                                             \
    X(opc, op, type, F, 16777216.0f, 2.0f)

#define SIMPLE_ARITH_FLOAT_BINARY_CASES_64(X, opc, op, type, F)                                                        \
    X(opc, op, type, F, 4.2, 7.3)                                                                                      \
    X(opc, op, type, F, 0.0, -0.0)                                                                                     \
    X(opc, op, type, F, -0.0, -0.0)                                                                                    \
    X(opc, op, type, F, std::numeric_limits<double>::quiet_NaN(), 12.34)                                               \
    X(opc, op, type, F, std::numeric_limits<double>::infinity(), 12.34)                                                \
    X(opc, op, type, F, std::numeric_limits<double>::infinity(), 0.0)                                                  \
    X(opc, op, type, F, -std::numeric_limits<double>::infinity(), 12.34)                                               \
    X(opc, op, type, F, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity())             \
    X(opc, op, type, F, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN())             \
    X(opc, op, type, F, std::numeric_limits<double>::max(), 2.0)                                                       \
    X(opc, op, type, F, std::numeric_limits<double>::max(), -std::numeric_limits<double>::max())                       \
    X(opc, op, type, F, 9007199254740992.0, 1.0)                                                                       \
    X(opc, op, type, F, 9007199254740992.0, 2.0)

#define SIMPLE_ARITH_FLOAT_BINARY_CASES(X)                                                                             \
    X(SIMPLE_ARITH_FLOAT_BINARY_CASES_32, float, F32)                                                                  \
    X(SIMPLE_ARITH_FLOAT_BINARY_CASES_64, double, F64)

#define SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST(opc, op, type, F, l, r)                                                    \
    {                                                                                                                  \
        auto res    = InterpretFPRes(RewriteMethod(engine, path, "default", "test_" #opc)->code, F(l), F(r));          \
        type resVal = *(reinterpret_cast<const type*>(&res));                                                          \
        type expVal = (l)op(r);                                                                                        \
        if (std::isnan(resVal) && std::isnan(expVal)) {                                                                \
            SUCCEED();                                                                                                 \
        } else {                                                                                                       \
            ASSERT_EQ(res.u64, F(expVal).u64);                                                                         \
        }                                                                                                              \
    }

#define SIMPLE_ARITH_FLOAT_BINARY_TEST(CASES, type, F)                                                                 \
    TEST_ASM(CbcTest, SimpleArithFloatBinary##F)                                                                       \
    {                                                                                                                  \
        auto path    = "./simple_arith_float/simple_arith_float_binary" #F ".asm";                                     \
        auto& engine = OpenFile(path);                                                                                 \
        CASES(SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST, ADD, +, type, F)                                                    \
        CASES(SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST, SUB, -, type, F)                                                    \
        CASES(SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST, MUL, *, type, F)                                                    \
        CASES(SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST, DIV, /, type, F)                                                    \
    }

SIMPLE_ARITH_FLOAT_BINARY_CASES(SIMPLE_ARITH_FLOAT_BINARY_TEST)

#undef SIMPLE_ARITH_FLOAT_BINARY_CASES_32
#undef SIMPLE_ARITH_FLOAT_BINARY_CASES_64
#undef SIMPLE_ARITH_FLOAT_BINARY_CASES
#undef SIMPLE_ARITH_FLOAT_BINARY_CASE_TEST
#undef SIMPLE_ARITH_FLOAT_BINARY_TEST

#define SIMPLE_ARITH_FLOAT_UNARY_CASES_32(X, opc, op, type, F)                                                         \
    X(opc, op, type, F, 4.2f)                                                                                          \
    X(opc, op, type, F, 0.0f)                                                                                          \
    X(opc, op, type, F, -0.0f)                                                                                         \
    X(opc, op, type, F, std::numeric_limits<float>::quiet_NaN())                                                       \
    X(opc, op, type, F, std::numeric_limits<float>::infinity())                                                        \
    X(opc, op, type, F, -std::numeric_limits<float>::infinity())                                                       \
    X(opc, op, type, F, std::numeric_limits<float>::max())                                                             \
    X(opc, op, type, F, std::numeric_limits<float>::min())                                                             \
    X(opc, op, type, F, 16777216.0f)

#define SIMPLE_ARITH_FLOAT_UNARY_CASES_64(X, opc, op, type, F)                                                         \
    X(opc, op, type, F, 4.2)                                                                                           \
    X(opc, op, type, F, 0.0)                                                                                           \
    X(opc, op, type, F, -0.0)                                                                                          \
    X(opc, op, type, F, std::numeric_limits<double>::quiet_NaN())                                                      \
    X(opc, op, type, F, std::numeric_limits<double>::infinity())                                                       \
    X(opc, op, type, F, -std::numeric_limits<double>::infinity())                                                      \
    X(opc, op, type, F, std::numeric_limits<double>::max())                                                            \
    X(opc, op, type, F, std::numeric_limits<double>::min())                                                            \
    X(opc, op, type, F, 9007199254740992.0)

#define SIMPLE_ARITH_FLOAT_UNARY_CASES(X)                                                                              \
    X(SIMPLE_ARITH_FLOAT_UNARY_CASES_32, float, F32)                                                                   \
    X(SIMPLE_ARITH_FLOAT_UNARY_CASES_64, double, F64)

#define SIMPLE_ARITH_FLOAT_UNARY_CASE_TEST(opc, op, type, F, v)                                                        \
    {                                                                                                                  \
        auto res    = InterpretFPRes(RewriteMethod(engine, path, "default", "test_" #opc)->code, F(v), F(0));          \
        type resVal = *(reinterpret_cast<const type*>(&res));                                                          \
        type expVal = op(v);                                                                                           \
        if (std::isnan(resVal) && std::isnan(expVal)) {                                                                \
            SUCCEED();                                                                                                 \
        } else {                                                                                                       \
            ASSERT_EQ(res.u64, F(expVal).u64);                                                                         \
        }                                                                                                              \
    }

#define SIMPLE_ARITH_FLOAT_UNARY_TEST(CASES, type, F)                                                                  \
    TEST_ASM(CbcTest, SimpleArithFloatUnary##F)                                                                        \
    {                                                                                                                  \
        auto path    = "./simple_arith_float/simple_arith_float_unary" #F ".asm";                                      \
        auto& engine = OpenFile(path);                                                                                 \
        CASES(SIMPLE_ARITH_FLOAT_UNARY_CASE_TEST, NEG, -, type, F)                                                     \
        CASES(SIMPLE_ARITH_FLOAT_UNARY_CASE_TEST, SQRT, std::sqrt, type, F)                                            \
        CASES(SIMPLE_ARITH_FLOAT_UNARY_CASE_TEST, ABS, std::abs, type, F)                                              \
    }

SIMPLE_ARITH_FLOAT_UNARY_CASES(SIMPLE_ARITH_FLOAT_UNARY_TEST)

#undef SIMPLE_ARITH_FLOAT_UNARY_CASES_32
#undef SIMPLE_ARITH_FLOAT_UNARY_CASES_64
#undef SIMPLE_ARITH_FLOAT_UNARY_CASES
#undef SIMPLE_ARITH_FLOAT_UNARY_CASE_TEST
#undef SIMPLE_ARITH_FLOAT_UNARY_TEST

TEST_ASM(CbcTest, SimpleArithFloatMov32)
{
    auto path    = "./simple_arith_float/simple_arith_float_movF32.asm";
    auto& engine = OpenFile(path);

    {
        auto res = InterpretFPRes(RewriteMethod(engine, path, "default", "test_MOV")->code, F32(0), F32(2.0f));
        ASSERT_EQ(res.f32, 2.0f);
    }
    {
        auto res =
            Interpret(RewriteMethod(engine, path, "default", "test_F2I")->code, U32(0), U32(0), F32(2.5f), F32(0));
        ASSERT_EQ(res.u32, 0x40200000);
    }
    {
        auto res = InterpretFPRes(
            RewriteMethod(engine, path, "default", "test_I2F")->code, U32(0x40200000), U32(0), F32(0), F32(0)
        );
        ASSERT_EQ(res.f32, 2.5f);
    }
}

TEST_ASM(CbcTest, SimpleArithFloatMov64)
{
    auto path    = "./simple_arith_float/simple_arith_float_movF64.asm";
    auto& engine = OpenFile(path);

    {
        auto res = InterpretFPRes(RewriteMethod(engine, path, "default", "test_MOV")->code, F64(0), F64(2.0));
        ASSERT_EQ(res.f64, 2.0);
    }
    {
        auto res =
            Interpret(RewriteMethod(engine, path, "default", "test_F2I")->code, U64(0), U64(0), F64(2.5), F64(0));
        ASSERT_EQ(res.u64, 0x4004000000000000);
    }
    {
        auto res = InterpretFPRes(
            RewriteMethod(engine, path, "default", "test_I2F")->code, U64(0x4004000000000000), U64(0), F64(0), F64(0)
        );
        ASSERT_EQ(res.f64, 2.5);
    }
}
