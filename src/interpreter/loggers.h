#pragma once

#include "utils/logger.h"

namespace Interpretation {
namespace Log {

/// Loggers for interpetation actions.
/// TRACE - intepretation start and end.
/// DEBUG - instruction logging.
///
/// NOTE: this logger is enabled only in debug builds.
/// TODO: separate define to enable in release builds.
extern Logging::Logger interpretation;

/// Logger for bytecode preparation actions.
/// TRACE - enable disasm for input and rewritten bytecodes
extern Logging::Logger preparation;
} // namespace Log
} // namespace Interpretation
