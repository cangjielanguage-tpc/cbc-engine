
#include "assertion.h"
#include <stdarg.h>
#include <stdio.h>

[[noreturn]]
void ReportFailure(const char* filename, int line, const char* func, const char* fmt, ...)
{
    fprintf(stderr, "%s:%d: assertion failed in %s: ", filename, line, func);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
    ASSERTION_TRAP();
}
