#pragma once

#include "utils/logger.h"
#include <cstdint>

namespace Interpretation {
namespace Log {

/// Loggers for interpetation actions.
/// DEBUG - intepretation start and end.
/// TRACE - instruction logging.
///
/// NOTE: this logger is enabled only in debug builds.
/// TODO: separate define to enable in release builds.
extern Stream::Descripted stream;
extern Logging::Logger interpretation;
extern int64_t skipThreshold;

/// Logger for bytecode preparation actions.
/// TRACE - enable disasm for input and rewritten bytecodes
/// INFO - preparation notifications
extern Logging::Logger preparation;
} // namespace Log
} // namespace Interpretation
