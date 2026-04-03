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

static Interpretation::ExecBytecodeInfo* OpenAndRewrite(std::string_view name, std::string_view fileName)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto mainId      = engine.FindMain(session, fileName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh    = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, mainId.value()));
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
