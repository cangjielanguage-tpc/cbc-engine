
#include "assertion.h"
#include <stdarg.h>
#include <stdio.h>

[[noreturn]]
void ReportFailure(const char* filename, int line, const char* func, const char* fmt, ...)
{
    fprintf(stderr, "%s:%d: assertion failed in %s: ", filename, line, func);
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
