#ifndef MOCK_INTERPRETER_H
#define MOCK_INTERPRETER_H

#include "interpreter/code.h"
#include "interpreter/ectype.h"
#include "interpreter/interpreter.h"

struct TestTypeInfo {
    size_t size;
};

static Interpretation::Value::Primitive U32(uint32_t v) { return Interpretation::Value::Primitive { .u32 = v }; }

static Interpretation::Value::Primitive U64(uint64_t v) { return Interpretation::Value::Primitive { .u64 = v }; }

Interpretation::Value::Primitive
Interpret(Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2);

Interpretation::Value::Primitive
InterpretFPRes(Interpretation::Code code, Interpretation::Value::Primitive ir1, Interpretation::Value::Primitive ir2);

Interpretation::Value::Primitive Interpret(
    Interpretation::Code code,
    Interpretation::Frame* frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2
);

Interpretation::Value::Primitive InterpretFPRes(
    Interpretation::Code code,
    Interpretation::Frame* frame,
    Interpretation::Value::Primitive ir1,
    Interpretation::Value::Primitive ir2
);

#endif // MOCK_INTERPRETER_H
