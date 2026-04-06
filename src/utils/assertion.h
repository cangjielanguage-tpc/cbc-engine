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
    #include <stdio.h>
    #define ASSERT(...)                                                                                                \
        do {                                                                                                           \
            if (__VA_ARGS__) {                                                                                         \
            } else {                                                                                                   \
                fprintf(                                                                                               \
                    stderr,                                                                                            \
                    "%s:%d: assertion failed in %s: %s\n",                                                             \
                    __FILE__,                                                                                          \
                    __LINE__,                                                                                          \
                    CBC_ENGINE_FUNC_NAME,                                                                              \
                    #__VA_ARGS__                                                                                       \
                );                                                                                                     \
                fflush(stderr);                                                                                        \
                ASSERTION_TRAP();                                                                                      \
            }                                                                                                          \
        } while (false)
    #define ASSERTION(cond, ...)                                                                                       \
        do {                                                                                                           \
            if (cond) {                                                                                                \
            } else {                                                                                                   \
                fprintf(stderr, "%s:%d: assertion failed in %s: ", __FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);          \
                fprintf(stderr, __VA_ARGS__);                                                                          \
                fprintf(stderr, "\n");                                                                                 \
                fflush(stderr);                                                                                        \
                ASSERTION_TRAP();                                                                                      \
            }                                                                                                          \
        } while (false)
    #define FATAL(...)                                                                                                 \
        fprintf(stderr, "%s:%d: fatal error in %s: ", __FILE__, __LINE__, CBC_ENGINE_FUNC_NAME);                       \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fprintf(stderr, "\n");                                                                                         \
        fflush(stderr);                                                                                                \
        ASSERTION_TRAP()
#endif // defined(UNIT_TEST_MODE)
