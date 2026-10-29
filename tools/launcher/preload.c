/*
 * LD_PRELOAD library that enables the CBC interpreter for AOT binaries.
 *
 * The main-branch Cangjie runtime initializes the interpreter only via the
 * launcher (InitCJInterpreter API). This library restores the in-process
 * initialization path used by the hotfix flow: it intercepts
 * CJ_MRT_CjRuntimeInit (the runtime initialization call made by the binary
 * entry point), forwards it to the real implementation, then initializes the
 * CBC interpreter — the same sequence the launcher performs.
 *
 * The interpreter library and its options (cbc.path, etc.) are configured
 * through the CBCOPT environment variable, which the engine reads on init.
 *
 * Build: see build-amd64.sh. Usage:
 *   LD_PRELOAD=<tools/lib>/libcbc-preload.so CBCOPT="cbc.path=<dir>" ./binary
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#include "engine-common.h"

void CJ_MRT_CjRuntimeInit(void) {
    static void (*real_init)(void) = NULL;
    if (!real_init) {
        real_init = (void (*)(void))dlsym(RTLD_NEXT, "CJ_MRT_CjRuntimeInit");
    }

    if (real_init) {
        real_init();
    } else {
        fprintf(stderr, "[cbc-preload] CJ_MRT_CjRuntimeInit not found, skipping interpreter init\n");
        return;
    }

    enum RTErrorCode code = init_cbc_interpreter();
    if (code != E_OK) {
        fprintf(stderr, "[cbc-preload] interpreter initialization failed with code: %d\n", code);
    }
}
