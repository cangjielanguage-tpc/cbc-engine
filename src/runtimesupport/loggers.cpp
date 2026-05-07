#include "loggers.h"
#include "utils/ostream.h"

Stream::Descripted typeinfoStream(Stream::cerr, "[TI] ");
Logging::Logger RTSupport::Log::typeinfo;

