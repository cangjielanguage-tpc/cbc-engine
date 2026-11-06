#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/implicit_exceptions.h"
#include "interpreter/int_thunk.h"
#include "interpreter/interpreter.h"
#include "runtimesupport/runtime.h"
#include "utils/vector.h"

struct MockInterfaceCall {
    RTSupport::TypeInfo receiver;
    RTSupport::TypeInfo reference;
    int methodNum;
};

struct MockInterfaceDispatch {
    bool enabled = false;
    RTSupport::TypeInfo outerTi;
    Interpretation::Thunk thunk {};
    Utils::Vector<MockInterfaceCall> outerTiCalls;
    Utils::Vector<MockInterfaceCall> dispatchCalls;
};

MockInterfaceDispatch& InterfaceDispatchMock();

struct TestTypeInfo {
    size_t size;
    uint8_t alignment;
    Utils::Vector<uint32_t> referenceOffsets;
    // Enum-like terms (Image::EnumKind::OPTION0/OPTION1) model overlapping alternatives: every
    // field ordinal is at data offset 0, so Execution::GetFieldOffset ignores the ordinal
    // (this is what keeps the rewriter's payload.g lowering - LEA_GENERIC of ordinal 1 plus
    // the object header - pointing at the payload word behind the header).
    bool overlappingFields;

    TestTypeInfo(
        size_t size,
        uint8_t alignment = alignof(std::max_align_t),
        Utils::Vector<uint32_t> referenceOffsets = {},
        bool overlappingFields = false
    )
        : size(size),
          alignment(alignment),
          referenceOffsets(std::move(referenceOffsets)),
          overlappingFields(overlappingFields)
    {}
};

static Interpretation::Value::Primitive U32(uint32_t v) { return Interpretation::Value::Primitive { .u32 = v }; }

template <typename T>
static Interpretation::Value::Primitive U64(T v) { return Interpretation::Value::Primitive { .u64 = static_cast<uint64_t>(v) }; }

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

// Runs the code over a test-provided frame buffer and returns the result.
// `frame.start` must point one past the last frame slot (see the geometry note in
// test/mock/interpreter.cpp); `frameBot` must then be frame.start - frameSize
// (from ExecBytecodeInfo::frameSize) so that stack-passed call arguments staged by
// st.stack.param*/lea.stack.param land inside the provided buffer. Defaults to 0,
// which keeps the historical behavior (fine for code without stack arguments).
Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    uintptr_t frameBot = 0
);

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive ir3
);

// Like Interpret, but reads the result from IReg::IR_ACC - the register the
// engine's ACC-form ops place results in (NEWOBJ_ACC_G thunk, todo-mock 3.4).
// The acc forms have no DSL spelling, so IR_ACC observability is exercised
// through flows that allocate on ACC internally (new.none.g/new.some.g).
Interpretation::Value::Primitive InterpretAccRes(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
);

// Runs the code and returns the type of the implicit exception it throws
// (Interpretation::Type::Successful if it runs to completion).
Interpretation::Type InterpretThrows(
    Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2
);

Interpretation::Type InterpretThrows(
    Interpretation::Code code,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2,
    Interpretation::Value::Primitive ir3
);

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame frame,
    Interpretation::Value::Primitive fr0,
    Interpretation::Value::Primitive fr1,
    uintptr_t frameBot = 0
);

// Base regions returned by RTSupport::Execution::GetLocalBasePtr/GetGlobalBasePtr (the
// rewrite-time queries behind movbp.local/.global and lea.static): two distinct static
// mock-owned buffers. They have engine lifetime, like the synthetic-TI registry - rewritten
// bytecode of any test embeds their addresses as immediates, so they must outlive every
// engine. Tests pre-seed and inspect them directly through these accessors; the mock zeroes
// both areas in InitializeMockInterpreter.
void* MockLocalBaseArea();
void* MockGlobalBaseArea();

void InitializeMockInterpreter();
