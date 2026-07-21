#pragma once

#include "utils/ostream.h"

namespace Logging {

/// Logging utilities.
/// The class consist of an base logger class, that provides either direct access or lambda-wrapped to underlying
/// stream.
///
/// There are an number of different log levels, which determine whether the log would be printed or not.
/// Loggers are represented as globals in corresponding modules.

enum class Level : int {
    NONE,
    FATAL,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
    BLOCK,
    COUNT
};

class Logger {
public:
    Logger(Stream::Output* output, Level level = Level::NONE);
    Logger(Level level = Level::NONE);
    Stream::Output& Stream(Level level);
    void SetStream(Stream::Output* stream);
    void SetLogLevel(Level level);
    Level GetLogLevel();

    template <typename F> inline void Log(Level level, F const& logger) { logger(*byLevel[(int)level]); }

private:
    Stream::Output* output;
    Stream::Output* byLevel[(uint32_t)Level::COUNT];
    Stream::Descripted descriptedByLevel[(uint32_t)Level::COUNT];
    Level level;
};

} // namespace Logging
