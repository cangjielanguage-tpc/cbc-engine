#include "loggers.h"
#include "utils/ostream.h"
#include <cstdint>

Stream::Descripted preparationStream(Stream::cerr, "[prep] ");
Stream::Descripted Interpretation::Log::stream(Stream::cerr, "[int] ");

Logging::Logger Interpretation::Log::interpretation(&stream);
Logging::Logger Interpretation::Log::preparation(&preparationStream);

int64_t Interpretation::Log::skipThreshold = -1;
