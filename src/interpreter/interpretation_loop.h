#pragma once

#include "cbc/decoder.h"
#include "ectype.h"
#include "frame.h"
#include "literals.h"
#include "runtimesupport/runtime.h"
#include "utils/logger.h"

namespace Interpretation {

struct Thunk {
    void* function;
    void* arg;
};

/// Logger for interpetation actions.
/// TRACE - intepretation start and end.
/// DEBUG - instruction logging.
///
/// NOTE: this logger is enabled only in debug builds.
/// TODO: separate define to enable in release builds.
extern Log::Logger g_Logger;

Thunk InterpretationLoop(
    Ectype* ectype, Frame frame, RTSupport::ThreadHandle handle, LiteralTable* literals, Decoder::ByteReader& reader0
);

} // namespace Interpretation
