#ifndef ENGINE_COMMON_H
#define ENGINE_COMMON_H

#include "engine.h"
#include "cj-interface.h"

/* Name of the interpreter (CBC engine) library used by both launcher and preload. */
extern char const *interpreter_lib;

/* Shared engine object with lazily resolved libcbcengine.so entry points. */
extern Engine g_engine;

/* dlopen() the interpreter library and resolve its entry points into g_engine. */
void load_engine_symbols(void);

/*
 * Initialize the CBC interpreter via the runtime's InitCJInterpreter API.
 * Resolves InitCJInterpreter dynamically so that the preload library does not
 * need libcangjie-runtime.so at load time.
 */
enum RTErrorCode init_cbc_interpreter(void);

#endif
