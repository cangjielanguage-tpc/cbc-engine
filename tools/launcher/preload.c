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
 * Build: python3 build.py build-launcher (or build-<arch>.sh on Linux). Usage:
 *   LD_PRELOAD=<tools/lib>/libcbc-preload.so CBCOPT="cbc.path=<dir>" ./binary
 *   (macOS: DYLD_INSERT_LIBRARIES=<tools/lib>/libcbc-preload.dylib ...)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#include "engine-common.h"

#ifdef __APPLE__
/*
 * macOS: two-level namespace binding has no PLT entries to redirect, and
 * dlsym(RTLD_NEXT) cannot reach libcangjie-runtime unless it is a link-time
 * dependency of this library. Use the dyld-native __DATA,__interpose section
 * instead: dyld rebinds every reference to CJ_MRT_CjRuntimeInit in all loaded
 * images to the replacement below. The real implementation is resolved via
 * normal linkage (the preload library links libcangjie-runtime).
 */
extern void CJ_MRT_CjRuntimeInit(void);

static void cbc_preload_CjRuntimeInit(void) {
    CJ_MRT_CjRuntimeInit();

    enum RTErrorCode code = init_cbc_interpreter();
    if (code != E_OK) {
        fprintf(stderr, "[cbc-preload] interpreter initialization failed with code: %d\n", code);
    }
}

__attribute__((used))
static struct { void *replacement; void *replacee; } cbc_interpose_CjRuntimeInit
__attribute__((section("__DATA,__interpose"))) = {
    (void *)&cbc_preload_CjRuntimeInit,
    (void *)&CJ_MRT_CjRuntimeInit,
};

#else /* ELF: classic symbol-override preload via RTLD_NEXT */

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

#endif
