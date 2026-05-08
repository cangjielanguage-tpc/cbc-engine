#include "rt_logger.h"
#include "utils/logger.h"
#include "utils/ostream.h"

// defined in the shared part to avoid linkage errors.

Stream::Descripted typeinfoStream(Stream::cerr, "[TI] ");
Logging::Logger RTSupport::Log::typeinfo(&typeinfoStream, Logging::Level::NONE);
