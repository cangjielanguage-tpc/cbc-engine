#include <gtest/gtest.h>

#include "api/resolver.h"
#include "cbc/formater_rt.h"
#include "engine/engine.h"
#include "engine/symlevel/io/byte_array_random_access_file.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "interpreter/function_handle.h"

#include "mock/interpreter.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class CbcTest : public testing::Test {
    void SetUp() override
    {
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

TEST_ASM(CbcTest, Simple)
{
    Engine::Loader loader;

    auto fileName   = "simple";
    auto file       = OpenAsm("simple.asm");
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto mainId      = engine.FindMain(session, fileName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh    = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, mainId.value()));
    auto bcInfo = fuhManager.Prepare(session, fuh);

    auto code = bcInfo->code;
    auto res  = Interpret(code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}

TEST_ASM(CbcTest, DirectCall)
{
    Engine::Loader loader;

    auto fileName   = "direct-call";
    auto file       = OpenAsm("direct-call.asm");
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto mainId      = engine.FindMain(session, fileName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh    = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, mainId.value()));
    auto bcInfo = fuhManager.Prepare(session, fuh);

    auto code = bcInfo->code;

    Cbc::RT::Log(code, std::cerr);
    auto res = Interpret(code, U32(0), U32(0));
    ASSERT_EQ(res.u32, 28);
}
