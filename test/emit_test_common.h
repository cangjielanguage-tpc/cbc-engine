#ifndef EMIT_TEST_COMMON_H
#define EMIT_TEST_COMMON_H

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "cbc/emitter/emitter.h"
#include "cbc/formater_rt.h"
#include "cbc/isa_rt.h"

#include "mock/interpreter.h"
#include "utils/math.h"
#include "testutils.h"
#include "utils/ostream.h"

inline LimitedHeap<16384> heap;

class EmitTest : public testing::Test {
    void SetUp() override
    {
        InitializeMockInterpreter();
        heap.Reset();
    }

    void TearDown() override {}
};

namespace Cbc {
namespace Emitter {

inline std::string LogCode(const Interpretation::Code& code)
{
    Stream::StringBuffer stream;
    RT::Log(code, stream);
    return stream.ToString();
}

inline std::string Hex(uint64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

inline std::string Hex(RTSupport::TypeInfo ti)
{
    return Hex(reinterpret_cast<uintptr_t>(ti.Raw()));
}

template <typename T>
inline std::string Hex(T* p)
{
    return Hex(reinterpret_cast<uintptr_t>(p));
}

inline std::string Pos(size_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%03llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

inline RTSupport::TypeInfo NewTestTypeInfo(size_t size)
{
    return RTSupport::TypeInfo(heap.New<TestTypeInfo>(size));
}

inline Decoder::ByteReader MakeReader(const Interpretation::Code& code)
{
    return Decoder::ByteReader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
}

// Asserts the disassembly of a code object line by line, e.g.:
//   CHECK_DISASM(code,
//       "0x000: add.32 IR1 IR1 IR2",
//       "0x003: ret");
template <typename... Lines>
inline testing::AssertionResult CheckDisasm(const Interpretation::Code& code, Lines const&... lines)
{
    std::string expected;
    ((expected += lines, expected += '\n'), ...);
    auto actual = LogCode(code);
    if (actual == expected) {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "disasm mismatch, actual:\n" << actual << "expected:\n" << expected;
}

#define CHECK_DISASM(...) EXPECT_TRUE(CheckDisasm(__VA_ARGS__))

} // namespace Emitter
} // namespace Cbc

#endif
