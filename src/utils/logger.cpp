#include "logger.h"
#include "utils/ostream.h"

namespace Logging {

class Blackhole : public Stream::Output {
    virtual void VPrintFmt(const char* fmt, va_list argp) { /* no-op */ }
};

Blackhole blackhole;

Logger::Logger(Stream::Output* output, Level level)
    : level(level),
      output(output),
      descriptedByLevel {
          Stream::Descripted(*output, ""),         Stream::Descripted(*output, "[FATAL] "),
          Stream::Descripted(*output, "[ERROR] "), Stream::Descripted(*output, "[WARN] "),
          Stream::Descripted(*output, "[INFO] "),  Stream::Descripted(*output, "[DEBUG] "),
          Stream::Descripted(*output, "[TRACE] "), Stream::Descripted(blackhole, ""),
      }
{
    SetLogLevel(level);
}

Logger::Logger(Level level) : Logger(&Stream::cerr, level) {}

Stream::Output& Logger::Stream(Level level) { return *byLevel[(int)level]; }

void Logger::SetStream(Stream::Output* stream) { this->output = stream; }

void Logger::SetLogLevel(Level level)
{
    this->level = level;
    for (int i = 0; i <= (int)level; i++) {
        byLevel[i] = &descriptedByLevel[i];
    }
    for (int i = (int)level + 1; i < (int)Level::COUNT; i++) {
        byLevel[i] = &blackhole;
    }
}

Level Logger::GetLogLevel() { return level; }

} // namespace Logging
