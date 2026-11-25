#include <gtest/gtest.h>

#include "engine/engine.h"
#include "engine/method_table.h"
#include "engine/terms.h"
#include "interpreter/function_handle.h"
#include "testutils.h"

namespace {

TEST(PrimitiveMethodTableTest, EmptyImagesDoNotRequireTypeDefinitions)
{
    Engine::Loader loader;
    Engine::Session session(loader.Build());
    auto& manager = Engine::MethodTableManager::Of(session);
    for (auto kind : { Engine::TermKind::UNIT, Engine::TermKind::BOOLEAN, Engine::TermKind::I64,
                      Engine::TermKind::F64, Engine::TermKind::UCHAR32, Engine::TermKind::BSTRING }) {
        auto table = manager.GetMethodTable(session, Engine::Term::Predefined(kind));
        ASSERT_TRUE(table.has_value());
        EXPECT_EQ((*table)->EntryCount(), 0);
        EXPECT_EQ((*table)->ClassCount(), 0);
        EXPECT_EQ((*table)->InterfaceCount(), 0);
    }
}

class HelperRegistrationTest : public testing::Test {};

TEST_ASM(HelperRegistrationTest, HandlesAreAvailableBeforeBodyPreparation)
{
    Engine::Loader loader;
    ASSERT_TRUE(loader.Load(OpenAsm("metadata-helpers.asm"), "metadata-helpers.asm"));
    Engine::Session session(loader.Build());
    auto& manager = Interpretation::FunctionHandleManager::Of(session);
    manager.FindHelpers(session);

    auto abstractCall = manager.GetHelper(Interpretation::Helper::ThrowAbstractMethodCallError);
    auto resolution = manager.GetHelper(Interpretation::Helper::ThrowSymbolResolutionError);
    ASSERT_NE(abstractCall, nullptr);
    ASSERT_NE(resolution, nullptr);
    EXPECT_EQ(abstractCall->bytecode.load(), nullptr);
    EXPECT_EQ(resolution->bytecode.load(), nullptr);

    // Metadata construction may request these handles before helper bodies are
    // rewritten. Re-registering later must preserve the same handles.
    manager.FindHelpers(session);
    EXPECT_EQ(manager.GetHelper(Interpretation::Helper::ThrowAbstractMethodCallError), abstractCall);
    EXPECT_EQ(manager.GetHelper(Interpretation::Helper::ThrowSymbolResolutionError), resolution);
}

} // namespace
