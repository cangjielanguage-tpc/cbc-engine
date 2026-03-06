#include <gtest/gtest.h>

#include "engine/engine.h"
#include "engine/symlevel/io/byte_array_random_access_file.h"
#include "engine/symlevel/reader.h"
#include "engine/symlevel/string.h"
#include "interpreter/function_handle.h"

#include "mock/interpreter.h"
#include "testutils.h"

#define UNIT_TEST_MODE 1

static std::unique_ptr<IO::ByteArrayRandomAccessFile> FromString(std::string_view view)
{
    char* data        = new char[view.size() + 1];
    data[view.size()] = 0;
    view.copy(data, view.size());

    return std::make_unique<IO::ByteArrayRandomAccessFile>(data, view.size());
}

TEST(CbcTest, Empty)
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

    auto pkgName    = "simple";
    auto file       = OpenAsm("simple.asm");
    bool successful = loader.Load(std::move(file), pkgName);
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto mainId      = engine.FindMain(session, pkgName);
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(engine);

    auto fuh    = static_cast<Interpretation::DynamicFunctionHandle*>(fuhManager.Acquire(session, mainId.value()));
    auto bcInfo = fuhManager.Prepare(session, fuh);

    auto code = bcInfo->code;
    auto res  = Interpret(code, U32(0), U32(10));
    ASSERT_EQ(res.u32, 28);
}
