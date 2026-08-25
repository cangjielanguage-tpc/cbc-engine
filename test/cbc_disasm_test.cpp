#include <gtest/gtest.h>
#include <sstream>

#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "engine/image/reader.h"
#include "interpreter/function_handle.h"

#include "testutils.h"
#include "engine/options.h"
#include "utils/ostream.h"

static LimitedHeap<16384> heap;

class CbcDisasmTest : public testing::Test {
    void SetUp() override
    {
        heap.Reset();
        Engine::InitEnvOptions();
    }

    void TearDown() override {}
};

static void CompareWith(std::string_view fileName, std::string const& expected)
{
    Engine::Loader loader;

    auto file       = OpenAsm(std::string(fileName));
    bool successful = loader.Load(std::move(file), fileName);
    ASSERT_TRUE(successful);

    auto& engine = loader.Build();
    Engine::Session session(engine);

    auto mainId = engine.FindMain(session, fileName);
    ASSERT_TRUE(mainId.has_value());

    auto def  = Decode::Read(session, mainId.value());
    auto code = Decode::Read(session, def.MethodCode().value());

    Resolution::Resolver resolver(session, mainId.value());

    Stream::StringBuffer stream;
    Cbc::Disasm(stream, code, &resolver);

    ASSERT_EQ(expected, stream.ToString());
}

TEST_ASM(CbcDisasmTest, VirtCall)
{
    std::string expected = "0: call.virtual IR1, @Foo.foo()Void (1, 1)\n"
                           "3: ret.W64 IR1\n";
    CompareWith("cbc-virt-call.asm", expected);
}
