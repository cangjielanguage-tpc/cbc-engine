#ifndef ENGINE_H
#define ENGINE_H

#include "cbc_engine.h"

/*
 * Function-pointer table for the libcbcengine C-API entries used by the
 * preload library, which resolves the engine dynamically (dlopen) instead of
 * linking it at build time.
 */
typedef struct {
    void*     interpreter_init;
    CbcStatus (*parse_options)(char const *text);
    CbcStatus (*set_cbcpath)(char const *cbc_path);
    CbcStatus (*set_main_cbc)(char const *main_cbc);
    CbcStatus (*set_kv_option)(char const *key, char const *value);
    CbcStatus (*set_symbol_resolver)(CbcDlsym dlsym);
    CbcStatus (*load)(void);
    CbcStatus (*preset_init_staging)(CbcDlsym resolver);
    CbcStatus (*preset_cjnative_builtins)(CbcDlsym resolver);
    CbcStatus (*apply_hotfix_patches)(void);
    CbcStatus (*preset_in_executable_resolver)(CbcDlsym *rloc);
} Engine;

#endif
