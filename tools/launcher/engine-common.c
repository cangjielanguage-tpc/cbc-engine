#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine-common.h"

#ifdef __APPLE__
char const *interpreter_lib = "libcbcengine.dylib";
#else
char const *interpreter_lib = "libcbcengine.so";
#endif

Engine g_engine;

static void *xdlsym(void *handle, char const *name) {
    void *result = dlsym(handle, name);
    if (!result) {
        fprintf(stderr, "dlsym(%s) failed: %s\n", name, dlerror());
        exit(-1);
    }
    return result;
}

void load_engine_symbols(void) {
    void *engine_handle = dlopen(interpreter_lib, RTLD_NOW);
    if (!engine_handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        exit(-1);
    }

    g_engine.parse_options = xdlsym(engine_handle, "engine_parse_options");
    g_engine.set_cbcpath = xdlsym(engine_handle, "engine_set_cbcpath");
    g_engine.set_main_cbc = xdlsym(engine_handle, "engine_set_main_cbc");
    g_engine.set_kv_option = xdlsym(engine_handle, "engine_set_kv_option");
    g_engine.set_symbol_resolver = xdlsym(engine_handle, "engine_set_symbol_resolver");
    g_engine.load = xdlsym(engine_handle, "engine_load");
    g_engine.preset_init_staging = xdlsym(engine_handle, "engine_preset_init_staging");
    g_engine.preset_cjnative_builtins = xdlsym(engine_handle, "engine_preset_cjnative_builtins");
    g_engine.apply_hotfix_patches = xdlsym(engine_handle, "engine_apply_hotfix_patches");
    g_engine.preset_in_executable_resolver = xdlsym(engine_handle, "engine_preset_in_executable_resolver");
    g_engine.interpreter_init = xdlsym(engine_handle, "interpreter_bridge_init");
}

enum RTErrorCode init_cbc_interpreter(void) {
    if (!g_engine.interpreter_init) {
        load_engine_symbols();
    }
    enum RTErrorCode (*init)(struct InterpreterParam *param) =
        (enum RTErrorCode (*)(struct InterpreterParam *))dlsym(RTLD_DEFAULT, "InitCJInterpreter");
    if (init == NULL) {
        fprintf(stderr, "dlsym(InitCJInterpreter) failed: %s\n", dlerror());
        return E_FAILED;
    }

    struct InterpreterParam interpParams = {
        .interpreterInitAddress = g_engine.interpreter_init,
        .interpreterArgsCount   = 0,
    };
    return init(&interpParams);
}
