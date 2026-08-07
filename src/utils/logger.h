#pragma once

#include "utils/ostream.h"

#include <cstdint>

namespace Logging {

/// Logging utilities.
/// The class consist of an base logger class, that provides either direct access or lambda-wrapped to underlying
/// stream.
///
/// There are an number of different log levels, which determine whether the log would be printed or not.
/// Loggers are represented as globals in corresponding modules.

#define LOG(level, log, ...) \
    (log).Log(level, [&](::Stream::Output& out) { out.PrintLn(__VA_ARGS__); })

#define LOGS(level, log, session, ...)                \
    (log).Log(level, [&](::Stream::Output& out_) {         \
            ::Stream::ResolvingOutput out((session), out_);\
            out.PrintLn(__VA_ARGS__);               \
    })

#define LOG_TRACE(...) LOG(::Logging::TRACE, __VA_ARGS__)
#define LOG_INFO(...)  LOG(::Logging::INFO, __VA_ARGS__)
#define LOG_ERROR(...) LOG(::Logging::ERROR, __VA_ARGS__)

#define LOGS_TRACE(...) LOGS(::Logging::TRACE, __VA_ARGS__)
#define LOGS_INFO(...)  LOGS(::Logging::INFO, __VA_ARGS__)
#define LOGS_ERROR(...) LOGS(::Logging::ERROR, __VA_ARGS__)

enum Level : int {
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

    template <typename F> inline void Log(Level level, F const& logger)
    {
        if (GetLogLevel() >= level) {
            logger(*byLevel[(int)level]);
        }
    }

private:
    Stream::Output* output;
    Stream::Output* byLevel[(uint32_t)Level::COUNT];
    Stream::Descripted descriptedByLevel[(uint32_t)Level::COUNT];
    Level level;
};

} // namespace Logging
