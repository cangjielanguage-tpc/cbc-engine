#include "loggers.h"
#include "utils/ostream.h"
#include <cstdint>

Stream::Descripted interpretationStream(Stream::cerr, "[int] ");
Stream::Descripted preparationStream(Stream::cerr, "[prep] ");
Logging::Logger Interpretation::Log::interpretation(&interpretationStream);
Logging::Logger Interpretation::Log::preparation(&preparationStream);

int64_t Interpretation::Log::skipThreshold = -1;
