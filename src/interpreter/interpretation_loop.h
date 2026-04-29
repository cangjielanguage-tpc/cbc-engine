#pragma once

#include "cbc/decoder.h"
#include "ectype.h"
#include "frame.h"
#include "interpreter/function_handle.h"
#include "literals.h"
#include "runtimesupport/runtime.h"

namespace Interpretation {

struct Thunk {
    void* function;
    void* arg;
};

void InterpretationStart(DynamicFunctionHandle* handle, Ectype* ectype);
void InterpretationEnd(DynamicFunctionHandle* handle, Ectype* ectype);

Thunk InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);

} // namespace Interpretation
