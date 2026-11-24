#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cbc_engine.h"
#include "cj-interface.h"

#define MAX_ARGS 1024
char *g_arg_buffer[MAX_ARGS]; // global args buffer

#ifndef INTERPRETER_LIB
#define INTERPRETER_LIB "libcbcengine.so"
#endif

char const *interpreter_lib = INTERPRETER_LIB;

extern int   InitCJRuntime(struct RuntimeParam *param);
extern enum RTErrorCode InitCJInterpreter(struct InterpreterParam* param);
extern int   LoadCJLibraryWithInit(const char *libName);
extern void *FindCJSymbol(const char *libName, const char *symbolName);
extern void *RunCJTask(const void *func, void *args);
extern int   GetTaskRet(const void *handle, void** ret);
extern enum RTErrorCode SetCJCommandLineArgs(int argc, char* argv[]);

typedef enum ParserState {
    PARSE,
    ERROR,
    MAIN_PARSED,
    HELP_PRINTED,
} ParserState;

struct Parser {
    char **args;
    char **args_end;
    char const *exe_name;
};


static void init_cangjie_runtime(int arg_count) {
    long int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    struct RuntimeParam rtParams = {
        .heapParam = {
            .regionSize = 64,
            .heapSize = 2 * 1024 * 1024,
            .exemptionThreshold= 0.8,
            .heapUtilization = 0.8,
            .heapGrowth = 0.15,
            .allocationRate = 0,
            .allocationWaitTime = 0,
        },
        .gcParam = {
            .gcThreshold = 0,
            .garbageThreshold = 0,
            .gcInterval = 0,
            .backupGCInterval = 0,
            .gcThreads = 0,
        },
        .logParam = {
            .logLevel = RTLOG_ERROR,
        },
        .coParam = {
            .thStackSize = 2 * 1024,
            .coStackSize = 64,
            .processorNum = (uint32_t) ncpu,
        },
    };

    struct InterpreterParam interpParams = {
        .interpreterLibName   = interpreter_lib,
        .interpreterArgsCount = 0,
    };

    int rtInitCode = InitCJRuntime(&rtParams);
    if (rtInitCode != 0) {
        fprintf(stderr, "Runtime initialization failed with code: %d\n", rtInitCode);
        exit(-1);
    }

    enum RTErrorCode interpInitCode = InitCJInterpreter(&interpParams);
    if (interpInitCode != E_OK) {
        fprintf(stderr, "Interpreter initialization failed with code: %d\n", interpInitCode);
        exit(-1);
    }

    SetCJCommandLineArgs(arg_count, g_arg_buffer);
}

static int run_interpreter_in_managed_ctx(void const *entryPoint) {
    void* fiberHandle = RunCJTask(entryPoint, NULL);
    if (fiberHandle == NULL) {
        fprintf(stderr, "Cangjie task creation failed\n");
        exit(-1);
    }

    long res;
    int runCode = GetTaskRet(fiberHandle, (void**) &(res));
    if (runCode != 0) {
        fprintf(stderr, "Managed execution failed with code: %d\n", runCode);
        exit(-1);
    }

    return (int) res;
}

static ParserState print_help(struct Parser *parser) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s [options] <main.cbc> args...\n", parser->exe_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "<main.cbc>\n");
    fprintf(stderr, "    CBC file which contains \"main\" function\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "[options]:\n");
    fprintf(stderr, "    --cbc-path <cbc-library-path>\n");
    fprintf(stderr, "        Paths to CBC libraries or directories which contain CBC libraries\n");
    fprintf(stderr, "        which should be loaded with the application.\n");
    fprintf(stderr, "        The paths should be separated with ':' character.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    --help\n");
    fprintf(stderr, "        Show this help message.\n");
    fprintf(stderr, "\n");
    return HELP_PRINTED;
}

static ParserState parse_cbc_path(struct Parser *parser) {
    assert(parser->args < parser->args_end);
    char **opt = parser->args;
    char **path = opt + 1;
    if (path < parser->args_end) {
        CbcStatus status = engine_set_cbcpath(*path);
        if (status != CBC_OK) {
            fprintf(stderr, "Cannot set cbc path '%s': %d\n", *path, status);
            return ERROR;
        }
        parser->args += 2;
        return PARSE;
    } else {
        return ERROR;
    }
}

static int parse_cbc_main(struct Parser *parser) {
    assert(parser->args < parser->args_end);
    char **main = parser->args;
    parser->args += 1;
    CbcStatus status = engine_set_main_cbc(*main);
    if (status != CBC_OK) {
        fprintf(stderr, "Cannot set main cbc '%s': %d\n", *main, status);
        return ERROR;
    }
    return MAIN_PARSED;
}

#ifndef CBC_HELPER_LIB
#if defined(__APPLE__)
#define CBC_HELPER_LIB "@rpath/libcbcengine-helper.dylib"
#else
#define CBC_HELPER_LIB "libcbcengine-helper.so"
#endif
#endif

/// Loads the engine helper library into the global scope so that the cjnative
/// builtins it provides are visible to the symbol resolver.
static void load_helper_library(void) {
    if (dlopen(CBC_HELPER_LIB, RTLD_GLOBAL | RTLD_LAZY) == NULL) {
        fprintf(stderr, "Warning: cannot load %s: %s\n", CBC_HELPER_LIB, dlerror());
    }
}

/// Loads CBC inputs and initializes the runtime in the order required by the
/// engine C-API (see `cbc_engine.h`).
static int start_with_engine(int arg_count) {
    CbcDlsym resolver;
    CbcStatus status = engine_preset_in_executable_resolver(&resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "Cannot create the executable symbol resolver: %d\n", status);
        return -1;
    }

    status = engine_set_symbol_resolver(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "Cannot set the symbol resolver: %d\n", status);
        return -1;
    }

    status = engine_load();
    if (status != CBC_OK) {
        fprintf(stderr, "Engine load failed: %d\n", status);
        return -1;
    }

    init_cangjie_runtime(arg_count);

    status = engine_preset_init_staging(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "Staging initialization failed: %d\n", status);
        return -1;
    }

    load_helper_library();

    status = engine_preset_cjnative_builtins(resolver);
    if (status != CBC_OK) {
        fprintf(stderr, "Builtins initialization failed: %d\n", status);
        return -1;
    }

    CbcFunction main_fn;
    status = engine_query_main_function(&main_fn);
    if (status != CBC_OK) {
        fprintf(stderr, "Main function query failed: %d\n", status);
        return -1;
    }

    return run_interpreter_in_managed_ctx(main_fn.code);
}

static int parse_args_and_start(int argc, char **argv) {
    struct Parser parser = {
        .args = argv + 1,
        .args_end = argv + argc,
        .exe_name = argv[0],
    };

    struct Option {
        char const *literal;
        ParserState (*parse_func)(struct Parser *);
    };

    struct Option options[] = {
        {"--cbc-path", &parse_cbc_path},
        {"-cp", &parse_cbc_path},
        {"--help", &print_help},
        {"-h", &print_help},
    };
    struct Option *opt_end = options + (sizeof(options) / sizeof(struct Option));

    CbcStatus envStatus = engine_parse_options(getenv("CBCOPT"));
    if (envStatus != CBC_OK) {
        fprintf(stderr, "Cannot parse CBCOPT options: %d\n", envStatus);
        return -1;
    }

    ParserState state = PARSE;
dispatch:
    switch (state) {
        case PARSE: {
            if (parser.args >= parser.args_end) {
                state = ERROR;
                goto dispatch;
            }

            for (struct Option *opt = options; opt < opt_end; opt++) {
                if (strcmp(opt->literal, *parser.args) == 0) {
                    state = opt->parse_func(&parser);
                    goto dispatch;
                }
            }
            state = parse_cbc_main(&parser);
            goto dispatch;
        }
        case HELP_PRINTED: {
            return 0;
        }
        case ERROR: {
            print_help(&parser);
            return -1;
        }
        case MAIN_PARSED: {
            int arg_count = (int) (parser.args_end - parser.args) + 1;
            if (arg_count > MAX_ARGS) {
                fprintf(stderr, "too much arguments: %d\n", arg_count);
                return -1;
            }
            g_arg_buffer[0] = argv[0];
            for (char **cursor = g_arg_buffer + 1; parser.args < parser.args_end; ) {
                *cursor = *parser.args;
                parser.args++;
                cursor++;
            }

            return start_with_engine(arg_count);
        }
        default: return -1;
    }
}

int main(int argc, char *argv[]) {
    return parse_args_and_start(argc, argv);
}
