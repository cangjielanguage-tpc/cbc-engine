#ifndef UTILS_ASSERTION_H
#define UTILS_ASSERTION_H

#if defined(UNIT_TEST_MODE)
    #include <stdexcept>
    #define ASSERTION(cond, msg)                                                                                       \
        do {                                                                                                           \
            if (!(cond))                                                                                               \
                throw std::runtime_error(msg);                                                                         \
        } while (0)
    #define ASSERT(cond) ASSERTION(cond, "")
#else
    #include <cassert>
    #define ASSERT(cond) assert(cond)
    #define ASSERTION(cond, msg) assert((cond) && (msg))
#endif // defined(UNIT_TEST_MODE)

#endif // UTILS_ASSERTION_H
