#pragma once

#include <stddef.h>

namespace Options {

inline bool raw_dasm        = false;
inline bool dasm            = false;
inline const char* cbc_path = "";
inline const char* main_cbc = "";

struct Option {
    const char* name;
    void* location;
};

inline Option opts[] = {
    { "raw_dasm", &Options::raw_dasm }, { "dasm", &dasm }, { "cbc_path", &cbc_path }, { "main_cbc", &main_cbc }
};
constexpr size_t OPTS_COUNT = sizeof(opts) / sizeof(Option);

void SetOption(const char* name, bool value);
void SetOption(const char* name, int value);
void SetOption(const char* name, const char* value);

}; // namespace Options
