#include "utils/options.h"

namespace Options {

void SetOption(const char* name, bool value)
{
    size_t idx = 0;
    while (idx < OPTS_COUNT && opts[idx].name == name) {
        ++idx;
    }
    if (idx < OPTS_COUNT) {
        *(bool*)(opts[idx].location) = value;
    } else {
        // TODO: report not found message
        return;
    }
}

void SetOption(const char* name, int value)
{
    size_t idx = 0;
    while (idx < OPTS_COUNT && opts[idx].name == name) {
        ++idx;
    }
    if (idx < OPTS_COUNT) {
        *(bool*)(opts[idx].location) = value;
    } else {
        // TODO: report not found message
        return;
    }
}

void SetOption(const char* name, const char* value)
{
    size_t idx = 0;
    while (idx < OPTS_COUNT && opts[idx].name == name) {
        ++idx;
    }
    if (idx < OPTS_COUNT) {
        *(bool*)(opts[idx].location) = value;
    } else {
        // TODO: report not found message
        return;
    }
}

}; // namespace Options
