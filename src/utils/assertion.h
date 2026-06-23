#pragma once

#ifdef NDEBUG

    #define ASSERT(cond)                                                                                               \
        do {                                                                                                           \
            if (false) {                                                                                               \
                if (cond) {}                                                                                           \
            }                                                                                                          \
        } while (false)

    #define ASSERTION(cond, ...)                                                                                       \
        do {                                                                                                           \
            if (false) {                                                                                               \
                if (cond) {}                                                                                           \
            }                                                                                                          \
        } while (false)

    #define NOTNULL(expression) (expression)

    #define FATAL(...)                                                                                                 \
        if (false) {}

#else

    #ifdef CBC_ENGINE_IMMEDIATE_ASSERTION
        #define ASSERTION_TRAP() __builtin_trap()
    #else
        #include <cstdlib>
        #define ASSERTION_TRAP() std::abort()
    #endif

    #ifdef CBC_ENGINE_PRETTY_FUNC_NAME
        #define CBC_ENGINE_FUNC_NAME __PRETTY_FUNCTION__
    #else
        #define CBC_ENGINE_FUNC_NAME __func__
    #endif

    #include <stdarg.h>
    #include <stdio.h>

    #if defined(__APPLE__) && __has_include(<TargetConditionals.h>)
        #include <TargetConditionals.h>
    #endif

    #if defined(TARGET_OS_IOS) && TARGET_OS_IOS && __has_include(<os/log.h>)
        #include <os/log.h>
        #define CBC_ENGINE_IOS_OS_LOG 1
    #else
        #define CBC_ENGINE_IOS_OS_LOG 0
    #endif

[[noreturn]]
static void ReportFailure(const char* filename, int line, const char* func, const char* fmt, ...)
{
    fprintf(stderr, "%s:%d: assertion failed in %s: ", filename, line, func);
    va_list args;
    va_start(args, fmt);
    #if CBC_ENGINE_IOS_OS_LOG
    va_list logArgs;
    va_copy(logArgs, args);
    #endif
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);

    #if CBC_ENGINE_IOS_OS_LOG
    char message[4096];
    int written = vsnprintf(message, sizeof(message), fmt, logArgs);
    va_end(logArgs);
    if (written < 0) {
        snprintf(message, sizeof(message), "<failed to format assertion message>");
    }

    os_log_error(
        OS_LOG_DEFAULT, "%{public}s:%d: assertion failed in %{public}s: %{public}s", filename, line, func, message
    );
    #endif

    ASSERTION_TRAP();
}

    #define ASSERT(cond)                                                                                               \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME, "%s", #cond);                                  \
            }                                                                                                          \
        } while (false)

    #define ASSERTION(cond, ...)                                                                                       \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME, __VA_ARGS__);                                  \
            }                                                                                                          \
        } while (false)

    #define NOTNULL(expression)                                                                                        \
        ([&]() {                                                                                                       \
            auto _ptr = (expression);                                                                                  \
            if (!_ptr)                                                                                                 \
                ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME, "Expected non-null pointer");                  \
            return _ptr;                                                                                               \
        }())

    #define FATAL(...) ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME, __VA_ARGS__)

#endif // ifdef NDEBUG
