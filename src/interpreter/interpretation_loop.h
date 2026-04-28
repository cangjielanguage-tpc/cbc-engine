#pragma once

#include "cbc/decoder.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtimesupport/runtime.h"

namespace Interpretation {

struct Thunk {
    void* function;
    void* arg;
};

Thunk InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);

} // namespace Interpretation
