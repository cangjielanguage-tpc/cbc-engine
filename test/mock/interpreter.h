#pragma once

#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/interpreter.h"

struct TestTypeInfo {
    size_t size;

    TestTypeInfo(size_t size) : size(size) {}
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
