#include "logger.h"
#include "utils/ostream.h"

namespace Log {

class Blackhole : public Stream::Output {
    virtual void VPrintFmt(const char* fmt, va_list argp) { /* no-op */ }
};

Blackhole blackhole;

Logger::Logger(Stream::Output* output) : level(Level::NONE), output(output) {}

Logger::Logger() : level(Level::NONE), output(&Stream::cerr) {}

Stream::Output& Logger::Stream(Level level)
{
    if (level <= this->level) {
        return *this->output;
    } else {
        return blackhole;
    }
}

void Logger::SetStream(Stream::Output* stream) { this->output = stream; }

void Logger::SetLogLevel(Level level) { this->level = level; }

} // namespace Log
