#include <stddef.h>

// Options data

bool opt_raw_dasm = false;
bool opt_dasm = false;
const char* cbc_path = "";
const char* main_cbc = "";


// Options representation

struct Option {
    const char* name;
    void* location;

};

Option opts[] = {
    {"raw_dasm", &opt_raw_dasm },
    {"dasm", &opt_dasm },
    {"cbc_path", &cbc_path },
    {"main_cbc", &main_cbc }
};
constexpr size_t OPTS_COUNT = sizeof(opts) / sizeof(Option);

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
