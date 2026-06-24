#pragma once

#include "cbc/decoder.h"
#include "ectype.h"
#include "frame.h"
#include "interpreter/function_handle.h"
#include "literals.h"
#include "runtimesupport/runtime.h"
#include <cstdint>

namespace Interpretation {

struct Thunk {
    void* function;
    void* arg;
};

enum BuiltinType : uint8_t {
    BUILTIN_BOOLEAN,
    BUILTIN_U8,
    BUILTIN_I8,
    BUILTIN_U16,
    BUILTIN_I16,
    BUILTIN_U32,
    BUILTIN_I32,
    BUILTIN_U64,
    BUILTIN_I64,
    BUILTIN_F16,
    BUILTIN_F32,
    BUILTIN_F64,
};

static constexpr auto BUILTIN_COUNT = BUILTIN_F64 + 1;

// Used by interpretation loop to access type infos of builtin types.
// Initialized by engine before interpretation.
extern RTSupport::TypeInfo builtinTypeInfos[BUILTIN_COUNT];

void InterpretationStart(DynamicFunctionHandle* handle, Ectype* ectype);
void InterpretationEnd(DynamicFunctionHandle* handle, Ectype* ectype);

Thunk InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);

} // namespace Interpretation
