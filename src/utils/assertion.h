#pragma once

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

#if defined(__APPLE__) && __has_include(<TargetConditionals.h>)
    #include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IOS) && TARGET_OS_IOS && __has_include(<os/log.h>)
    #include <os/log.h>
    #define ASSERTION_IOS_OS_LOG 1
#else
    #define ASSERTION_IOS_OS_LOG 0
#endif

#define FATAL(...) ReportFailure(__FILE__, __LINE__, __VA_ARGS__)

[[noreturn]]
void ReportFailure(const char* filename, int line, const char* fmt, ...);

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

#else

    #define ASSERT(cond)                                                                                               \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, "%s", #cond);                                                        \
            }                                                                                                          \
        } while (false)

    #define ASSERTION(cond, ...)                                                                                       \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                ReportFailure(__FILE__, __LINE__, __VA_ARGS__);                                                        \
            }                                                                                                          \
        } while (false)

    #define NOTNULL(expression)                                                                                        \
        ([&]() {                                                                                                       \
            auto _ptr = (expression);                                                                                  \
            if (!_ptr)                                                                                                 \
                ReportFailure(__FILE__, __LINE__, "Expected non-null pointer");                                        \
            return _ptr;                                                                                               \
        }())
    #define UNWRAP_OPT(name, expression, handler)                                                                      \
        auto __##name = (expression);                                                                                  \
        if (!__##name.has_value()) {                                                                                   \
            handler();                                                                                                 \
            return;                                                                                                    \
        }                                                                                                              \
        auto name = __##name.value();

#endif // ifdef NDEBUG
