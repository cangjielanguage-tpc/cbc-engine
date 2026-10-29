#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine-common.h"

char const *interpreter_lib = "libcbcengine.so";

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

    g_engine.get_trampoline = xdlsym(engine_handle, "engine_get_entrypoint_trampoline");
    g_engine.initialize = xdlsym(engine_handle, "engine_initialize");
    g_engine.set_cbcpath = xdlsym(engine_handle, "engine_set_cbcpath");
    g_engine.set_main_cbc = xdlsym(engine_handle, "engine_set_main_cbc");
    g_engine.enable_dasm = xdlsym(engine_handle, "engine_enable_dasm");
    g_engine.enable_raw_dasm = xdlsym(engine_handle, "engine_enable_raw_dasm");
}

enum RTErrorCode init_cbc_interpreter(void) {
    enum RTErrorCode (*init)(struct InterpreterParam *param) =
        (enum RTErrorCode (*)(struct InterpreterParam *))dlsym(RTLD_DEFAULT, "InitCJInterpreter");
    if (init == NULL) {
        fprintf(stderr, "dlsym(InitCJInterpreter) failed: %s\n", dlerror());
        return E_FAILED;
    }

    struct InterpreterParam interpParams = {
        .interpreterLibName   = interpreter_lib,
        .interpreterArgsCount = 0,
    };
    return init(&interpParams);
}
