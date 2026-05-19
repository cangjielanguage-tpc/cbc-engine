#pragma once

#include "utils/logger.h"

namespace RTSupport {
namespace Log {

/// Logger for typeinfo factory.
/// TRACE - logs factory method calls
/// INFO - logs information about the type, being created
/// ERROR - logs errors
extern Logging::Logger typeinfo;

extern Logging::Logger init;

/// Logger for gc-related operations.
/// TRACE - logs reference placeholders visiting
/// INFO  - logs gc-related operations calls
extern Logging::Logger gc;

extern Logging::Logger rt;

} // namespace Log
} // namespace RTSupport
