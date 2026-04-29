#include "loggers.h"
#include "utils/ostream.h"

Stream::Descripted interpretationStream(Stream::cerr, "[int] ");
Stream::Descripted preparationStream(Stream::cerr, "[preparation] ");
Logging::Logger Interpretation::Log::interpretation(&interpretationStream);
Logging::Logger Interpretation::Log::preparation(&preparationStream);
