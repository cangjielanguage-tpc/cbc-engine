#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "engine.h"

void trampolineil(void) __asm__("_CGP10trampolineilHv") __attribute__((used));
void trampolineil(void) { /* not used */ }

void trampolineii(void) __asm__("_CGP10trampolineiiHv") __attribute__((used));
void trampolineii(void) { /* not used */ }

/// Executed from the main thread in proper managed context
int goto_trampoline(void) __asm__("_CN10trampoline15goto_trampolineHv") __attribute__((used));
int goto_trampoline(void) {
    Engine* g_engine = (Engine*) dlsym(RTLD_DEFAULT, "g_engine"); 
    if (!g_engine) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        exit(-1);
    }

    void *trampoline = g_engine->get_trampoline();
    if (!trampoline) {
        fprintf(stderr, "entrypoint not found\n");
        return -1;
    }

    int (*trampoline0)(void) = trampoline;

    // 1) Avoid pure-c frames in managed context.
    // 2) As consequence of tail call the content of callee-saved registers would be
    //    restored before `trampoline` call, including thread-local register.
    __attribute__((musttail)) return trampoline0();
}