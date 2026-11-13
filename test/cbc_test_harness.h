#pragma once

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "mock/interpreter.h"
#include "testutils.h"

inline void DoSetUp()
{
    Cbc::EnableRawDisasm();
    InitializeMockInterpreter();
}

class CbcTest : public testing::Test {
    void SetUp() override
    {
        Interpretation::Log::preparation.SetLogLevel(Logging::Level::TRACE);
        DoSetUp();
    }

    void TearDown() override {}
};

// Deliberately never destroyed: engines are process-wide, and the registry must outlive
// static destructors so that LeakSanitizer still sees every engine as reachable at exit.
// TODO: FIXME #182
inline std::vector<Engine::Engine*>& BuiltEngines()
{
    static auto* engines = new std::vector<Engine::Engine*>;
    return *engines;
}

inline Interpretation::ExecBytecodeInfo* OpenAndRewrite(std::string name, std::string_view fileName)
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

inline void testFPOps32(Interpretation::Value::Primitive res, float expVal)
{
    auto resVal = res.u32;
    if (std::isnan(resVal) && std::isnan(expVal)) {
        SUCCEED();
    } else {
        ASSERT_EQ(res.u32, F32(expVal).u32);
    }
}

inline void testFPOps64(Interpretation::Value::Primitive res, double expVal)
{
    auto resVal = res.u64;
    if (std::isnan(resVal) && std::isnan(expVal)) {
        SUCCEED();
    } else {
        ASSERT_EQ(res.u64, F64(expVal).u64);
    }
}
