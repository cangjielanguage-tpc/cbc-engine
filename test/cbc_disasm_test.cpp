#include <gtest/gtest.h>
#include <sstream>

#include "cbc/isa_disasm.h"
#include "cbc/isa_parser.h"
#include "engine/engine.h"
#include "engine/symlevel/definitions.h"
#include "engine/symlevel/reader.h"
#include "interpreter/function_handle.h"

#include "testutils.h"
#include "utils/ostream.h"

static LimitedHeap<16384> heap;

class CbcDisasmTest : public testing::Test {
    void SetUp() override { heap.Reset(); }

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

    auto def  = Symlevel::MethodDefinition::Resolve(session, mainId.value());
    auto code = Symlevel::Reader::Read(session, def.FileId(), def.GetCodeOffset());

    auto resolver = API::Resolver::Create(session, mainId.value());

    static constexpr size_t BUF_SIZE = 1024ull;
    Stream::StringBuffer stream;
    Cbc::Disasm(stream, code, resolver.get())->ParseAll();

    ASSERT_EQ(expected, stream.ToString());
}

TEST_ASM(CbcDisasmTest, VirtCall)
{
    std::string expected = "0: call.virtual IR1, 0, 1\n"
                           "4: ret.64 IR1\n";
    CompareWith("cbc-virt-call.asm", expected);
}
