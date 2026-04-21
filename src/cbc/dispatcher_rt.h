#pragma once

#include "decoder.h"
#include "interpreter/ectype.h"
#include "interpreter/frame.h"
#include "interpreter/literals.h"
#include "runtimesupport/runtime.h"

namespace Cbc {
namespace RT {

using Width = Cbc::Format::Width;
using namespace Interpretation;

struct Thunk {
    void* function;
    void* arg;
};

Thunk InterpretationLoop(
    Ectype* ectype, Frame* frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);

} // namespace RT
} // namespace Cbc
