/*
 * LD_PRELOAD library that enables the CBC interpreter for AOT binaries.
 *
 * The main-branch Cangjie runtime initializes the interpreter only via the
 * launcher (InitCJInterpreter API). This library restores the in-process
 * initialization path used by the hotfix flow: it intercepts
 * CJ_MRT_CjRuntimeInit (the runtime initialization call made by the binary
 * entry point), forwards it to the real implementation, then initializes the
 * CBC engine and the CBC interpreter — the same sequence the launcher performs
 * (see the documented sequence in cbc_engine.h).
 *
 * The engine configuration (cbc.path, etc.) is provided through the CBCOPT
 * environment variable and parsed with engine_parse_options.
 *
 * Build: python3 build.py build-launcher (or build-<arch>.sh on Linux). Usage:
 *   LD_PRELOAD=<tools/lib>/libcbc-preload.so CBCOPT="cbc.path=<dir>" ./binary
 *   (macOS: DYLD_INSERT_LIBRARIES=<tools/lib>/libcbc-preload.dylib ...)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine-common.h"

#ifndef CBC_HELPER_LIB
#if defined(__APPLE__)
#define CBC_HELPER_LIB "@rpath/libcbcengine-helper.dylib"
#else
#define CBC_HELPER_LIB "libcbcengine-helper.so"
#endif
#endif

/*
 * Full engine initialization for the preload flow. The executable resolver
 * covers the AOT binary itself and its transitive dependencies (the cangjie
 * runtime), which provide the staging symbol. The cjnative builtins live in
 * the engine helper library, which nothing links against, so it is loaded
 * into the global scope explicitly.
 */
static void cbc_preload_init_engine(void) {
    load_engine_symbols();

    CbcDlsym resolver;
    CbcStatus status = g_engine.preset_in_executable_resolver(&resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] cannot create the executable symbol resolver: %d\n", status);
        return;
    }

    status = g_engine.set_symbol_resolver(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] cannot set the symbol resolver: %d\n", status);
        return;
    }

    status = g_engine.parse_options(getenv("CBCOPT"));
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] cannot parse CBCOPT options: %d\n", status);
        return;
    }

    status = g_engine.load();
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] engine load failed: %d\n", status);
        return;
    }

    enum RTErrorCode code = init_cbc_interpreter();
    if (code != E_OK) {
        fprintf(stderr, "[cbc-preload] interpreter initialization failed with code: %d\n", code);
        return;
    }

    status = g_engine.preset_init_staging(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] staging initialization failed: %d\n", status);
        return;
    }

    if (dlopen(CBC_HELPER_LIB, RTLD_GLOBAL | RTLD_LAZY) == NULL) {
        fprintf(stderr, "[cbc-preload] warning: cannot load %s: %s\n", CBC_HELPER_LIB, dlerror());
    }

    status = g_engine.preset_cjnative_builtins(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] builtins initialization failed: %d\n", status);
        return;
    }

    status = g_engine.apply_hotfix_patches();
    if (status != CBC_OK) {
        fprintf(stderr, "[cbc-preload] hotfix patching failed: %d\n", status);
    }
}

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
    cbc_preload_init_engine();
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

    cbc_preload_init_engine();
}

#endif
