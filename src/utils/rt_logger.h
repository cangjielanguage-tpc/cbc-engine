#pragma once

#include "utils/logger.h"

namespace RTSupport {
namespace Log {

/// Logger for typeinfo factory.
/// TRACE - logs factory method calls
/// INFO - logs information about the type, being created
/// ERROR - logs errors
extern Logging::Logger typeinfo;

} // namespace Log
} // namespace RTSupport
