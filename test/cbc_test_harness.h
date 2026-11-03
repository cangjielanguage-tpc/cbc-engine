#ifndef CBC_TEST_HARNESS_H
#define CBC_TEST_HARNESS_H

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "cbc/isa_disasm.h"
#include "engine/engine.h"
#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/function_handle.h"
#include "interpreter/loggers.h"
#include "mock/interpreter.h"
#include "testutils.h"

inline LimitedHeap<16384> heap;

inline void DoSetUp()
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

inline std::string hex(uint64_t num)
{
    std::ostringstream message;
    message << "0x" << std::hex << num;
    return message.str();
}

struct CheckedArithPair {
    uint64_t lhs;
    uint64_t rhs;
};

inline std::vector<CheckedArithPair> Cross(const std::vector<uint64_t>& lhs, const std::vector<uint64_t>& rhs)
{
    std::vector<CheckedArithPair> pairs;
    for (auto l : lhs) {
        for (auto r : rhs) {
            pairs.push_back({ l, r });
        }
    }
    return pairs;
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

// Mirrors the width traits make() of the interpreter: make(utype) zero-extends
// narrow results into the 32-bit primitive, make(stype) sign-extends.
template <typename U> uint64_t CheckedMakeU(U value)
{
    if constexpr (sizeof(U) == sizeof(uint64_t)) {
        return static_cast<uint64_t>(value);
    } else {
        return static_cast<uint64_t>(static_cast<uint32_t>(value));
    }
}

template <typename S> uint64_t CheckedMakeS(S value)
{
    if constexpr (sizeof(S) == sizeof(uint64_t)) {
        return static_cast<uint64_t>(value);
    } else {
        return static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(value)));
    }
}

inline uint64_t CheckedSMax(int width) { return (uint64_t { 1 } << (width - 1)) - 1; }

inline uint64_t CheckedSMin(int width) { return uint64_t { 1 } << (width - 1); }

inline uint64_t CheckedUMax(int width) { return width == 64 ? ~uint64_t { 0 } : (uint64_t { 1 } << width) - 1; }

// Narrow results are stored as 32-bit primitives; the union upper half is not
// guaranteed to be zeroed by the interpreter. The s32/ref/64 kinds carry their
// result in the full 64-bit cell (the s32 load sign-extends into it), which a
// u32 comparison cannot observe.
inline void ExpectMemValue(const Interpretation::Value::Primitive& res, uint64_t expected, bool fullCell)
{
    if (fullCell) {
        EXPECT_EQ(res.u64, expected);
    } else {
        EXPECT_EQ(res.u32, static_cast<uint32_t>(expected));
    }
}

#endif
