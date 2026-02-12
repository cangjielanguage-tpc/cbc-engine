#include <gtest/gtest.h>

#define UNIT_TEST_MODE 1

#include "cbc/emitter/emitter.h"
#include "cbc/rewriter.h"
#include "cbc/isa.h"

#include "mock/interpreter.h"
#include "mock/symlevel.h"
#include "testutils.h"

static LimitedHeap<16384> heap;

class EmitTest : public testing::Test {
    void SetUp() override {
        heap.Reset();
    }

    void TearDown() override {

    }
};

namespace Cbc {

struct Test;

using namespace Cbc::Format;

TEST(EmitTest, Rewriter_Simple) {
    uint8_t isa12Bytes[] = {0b00000000, 0b00100001, // Add IR1, IR2
                            0b10100100, 0b00011000  // Ret IR1
                            };
    Decoder::ByteReader isa12stream(isa12Bytes, isa12Bytes, isa12Bytes + 4);
    
    API::Fake::Method fakeMethod;
    Emitter::Emitter e;
    Rewriter rw(&fakeMethod, isa12stream, e);
    rw.Interpret();
    auto code = e.Build(heap);
    EXPECT_EQ(4, code.bytecodeSize);

    auto res = Interpret(code, U32(1), U32(2));
    EXPECT_EQ(res.u32, 3);
}

} // namespace Cbc
