#pragma once

#if defined(UNIT_TEST_MODE)
    #include <stdexcept>
    #define ASSERTION(cond, msg)                                                                                       \
        do {                                                                                                           \
            if (!(cond))                                                                                               \
                throw std::runtime_error(msg);                                                                         \
        } while (0)
    #define ASSERT(cond) ASSERTION(cond, "")
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

static void ReportFailure(const char* filename, int line, const char* func)
{
    fprintf(stderr, "%s:%d: assertion failed in %s: ", __FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);
}

static void ReportFailureDescription(const char* fmt...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

    #define ASSERT(cond)                                                                                               \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);                                               \
                ReportFailureDescription("%s", #cond);                                                                 \
                ASSERTION_TRAP();                                                                                      \
            }                                                                                                          \
        } while (false)
    #define ASSERTION(cond, ...)                                                                                       \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);                                               \
                ReportFailureDescription(__VA_ARGS__);                                                                 \
                ASSERTION_TRAP();                                                                                      \
            }                                                                                                          \
        } while (false)
    #define FATAL(...)                                                                                                 \
        ReportFailure(__FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);                                                       \
        ReportFailureDescription(__VA_ARGS__);                                                                         \
        ASSERTION_TRAP()
#endif // defined(UNIT_TEST_MODE)
