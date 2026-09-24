#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/int_thunk.h"
#include "interpreter/interpreter.h"
#include "runtimesupport/runtime.h"

struct MockInterfaceCall {
    RTSupport::TypeInfo receiver;
    RTSupport::TypeInfo reference;
    int methodNum;
};

struct MockInterfaceDispatch {
    bool enabled = false;
    RTSupport::TypeInfo outerTi;
    Interpretation::Thunk thunk {};
    std::vector<MockInterfaceCall> outerTiCalls;
    std::vector<MockInterfaceCall> dispatchCalls;
};

MockInterfaceDispatch& InterfaceDispatchMock();

struct TestTypeInfo {
    size_t size;
    uint8_t alignment;
    std::vector<uint32_t> referenceOffsets;

    TestTypeInfo(
        size_t size, uint8_t alignment = alignof(std::max_align_t), std::vector<uint32_t> referenceOffsets = {}
    )
        : size(size),
          alignment(alignment),
          referenceOffsets(std::move(referenceOffsets))
    {}
};

static Interpretation::Value::Primitive U32(uint32_t v) { return Interpretation::Value::Primitive { .u32 = v }; }

static Interpretation::Value::Primitive U64(uint64_t v) { return Interpretation::Value::Primitive { .u64 = v }; }

static Interpretation::Value::Primitive F32(float v) { return Interpretation::Value::Primitive { .f32 = v }; }

static Interpretation::Value::Primitive F64(double v) { return Interpretation::Value::Primitive { .f64 = v }; }

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
);

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code, Interpretation::Value::Primitive fr0, Interpretation::Value::Primitive fr1
);

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
);

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
);

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2
);

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1
);

void InitializeMockInterpreter();
