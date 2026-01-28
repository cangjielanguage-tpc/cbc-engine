#ifndef MOCK_INTERPRETER_H
#define MOCK_INTERPRETER_H

#include "interpreter/ectype.h"
#include "interpreter/code.h"
#include "interpreter/interpreter.h"

static Interpretation::Value::Primitive U32(uint32_t v) {
    return Interpretation::Value::Primitive{.u32 = v};
}

Interpretation::Value::Primitive Interpret(
        Interpretation::Code code,
        Interpretation::Value::Primitive ir1,
        Interpretation::Value::Primitive ir2);

#endif // MOCK_INTERPRETER_H

