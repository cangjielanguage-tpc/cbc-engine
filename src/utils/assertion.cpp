
#include "assertion.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

[[noreturn]]
void ReportFailure(const char* filename, int line, const char* fmt, ...)
{
    fprintf(stderr, "%s:%d: assertion failed: ", filename, line);
    va_list args;
    va_start(args, fmt);
#if ASSERTION_IOS_OS_LOG
    va_list logArgs;
    va_copy(logArgs, args);
#endif
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);

#if ASSERTION_IOS_OS_LOG
    constexpr size_t messageSize = 4096;
    char* messageBuffer          = static_cast<char*>(malloc(messageSize));
    if (messageBuffer != nullptr) {
        int written = vsnprintf(messageBuffer, messageSize, fmt, logArgs);
        if (written < 0) {
            snprintf(messageBuffer, messageSize, "<failed to format assertion message>");
        }
    }
    va_end(logArgs);

    const char* message = messageBuffer != nullptr ? messageBuffer : "<failed to allocate assertion message>";

    os_log_error(
        OS_LOG_DEFAULT, "%{public}s:%d: assertion failed in %{public}s: %{public}s", filename, line, func, message
    );
    free(messageBuffer);
#endif

    ASSERTION_TRAP();
}
