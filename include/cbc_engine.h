#ifndef CBC_ENGINE_H
#define CBC_ENGINE_H

#include <stdint.h>
#define CBC_EXPORT __attribute__((visibility("default"), used))

/// This file defines the C-API exposed to the cbc engine library users.
///
/// Example of a launcher initialization sequence:
///
///     // Configuration stage.
///     engine_parse_options(getenv("CBCOPT"));
///     engine_set_cbcpath("/path/to/cbc/libs");
///     engine_set_main_cbc("/path/to/app.cbc");
///
///     CbcDlsym resolver;
///     engine_preset_in_executable_resolver(&resolver);
///     engine_set_symbol_resolver(resolver);
///
///     // Load CBC image; `engine_query_function` becomes allowed.
///     engine_load();
///
///     // Runtime initialization stage: the runtime calls
///     // `interpreter_bridge_init` during `InitCJInterpreter`.
///     InitCJRuntime(&rtParams);
///     struct InterpreterParam params = {.interpreterLibName = "libcbcengine.so"};
///     InitCJInterpreter(&params);
///     engine_preset_init_staging(resolver);
///
///     // Library initialization stage: either preset AOT builtins
///     engine_preset_cjnative_builtins(resolver);
///     // or fill `CjBuiltins` manually (e.g. from CBC queries) and call
///     // engine_lib_initialize_builtins(&builtins);
///
///     // Obtain the entrypoint and run it through the runtime task API.
///     CbcFunction main;
///     engine_query_main_function(&main);
///     void* task = RunCJTask(main.code, args);

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CbcStatus {
    CBC_OK = 0,
    /// Null or malformed input parameter.
    CBC_INVALID_ARGUMENT,
    /// The call is not allowed in the current engine state, e.g. the engine is already loaded.
    CBC_INVALID_STATE,
    /// The requested term or method cannot be resolved.
    CBC_RESOLUTION_ERROR,
    /// The requested method query matches several methods.
    CBC_AMBIGUOUS_RESOLUTION_ERROR,
    /// The direct-call trampoline pool is exhausted.
    CBC_TRAMPOLINES_EXHAUSTED,
    /// The engine is not loaded; `engine_load` has not completed.
    CBC_NOT_LOADED,
    /// The main cbc is not set; `engine_set_main_cbc` was skipped.
    CBC_MAIN_NOT_SET,
    /// The runtime is not initialized; `interpreter_bridge_init` has not completed.
    CBC_RUNTIME_NOT_INITIALIZED,
    /// A cbc file does not exist or cannot be opened.
    CBC_FILE_NOT_FOUND,
    /// A symbol lookup performed through the provided resolver failed.
    CBC_SYMBOL_NOT_FOUND,
    /// A staging token is not recognized by the engine.
    CBC_UNKNOWN_TOKEN,
    /// An OS-level operation failed, e.g. `dlopen` or `dladdr`.
    CBC_PLATFORM_ERROR
} CbcStatus;

////////////////// Configuration stage. //////////////////

#define CBC_FIND_SYM_ERR_MAX_LEN 256

/// Resolve a symbol by the given `name`.
/// Return `null` in case of error and fill the buffer with a human readable error message.
typedef void* (*CbcFindSymbol)(void* ctx, char const* name, char buffer[CBC_FIND_SYM_ERR_MAX_LEN]);

typedef struct CbcDlsym {
    void* ctx;
    CbcFindSymbol find;
} CbcDlsym;

/// Set the folder where cbc libraries are contained.
/// The paths should be separated with `:` character.
///
/// Can be called repeatedly; new entries will be appended.
CBC_EXPORT CbcStatus engine_set_cbcpath(char const* cbc_path);

/// Parse an option zero-terminated string; options are separated by ` ` (space).
///
/// Note that in case of error, listed options could be partially applied.
CBC_EXPORT CbcStatus engine_parse_options(char const* text);

/// Parse a single key-value option.
CBC_EXPORT CbcStatus engine_set_kv_option(char const* key, char const* value);

/// Set the path to the main cbc that contains the `main` function.
CBC_EXPORT CbcStatus engine_set_main_cbc(char const* main_cbc);

/// Supply native lookup policy.
/// If set, the engine will use the provided function to query all symbols it requires:
/// - foreign functions;
/// - AOT compiled symbols;
///
/// The actual symbol resolution in binaries is decoupled from the cbcengine,
/// because it has many platform-specific and use-case-specific nuances.
CBC_EXPORT CbcStatus engine_set_symbol_resolver(CbcDlsym dlsym);

/// Load the provided CBC inputs without executing code or querying the runtime.
///
/// After that step, calls of `engine_query_function` are allowed.
CBC_EXPORT CbcStatus engine_load();

////////////////// Runtime initialization stage. //////////////////

/// The initialization of the interface for communication between the cbc engine and the runtime.
///
/// Deprecated:
/// - `int size`
/// - `const char** options` - supposed to be initialized using functions from the configuration stage.
CBC_EXPORT int interpreter_bridge_init(
    struct INT_InterpreterInterface* interpInterf,
    struct DYN_CJNativeInterface* rtInterf,
    int size,
    const char** options
);

#define READ_WEAK_REF_TOKEN "ReadWeakRef:0"

typedef struct {
    char const* token;
    void* symbol;
} CbcStagingSym;

/// The part of the runtime interface that has not yet become a part of the CJNativeInterface initialized above.
///
/// This interface is chosen to make staging symbols initialization as stable as possible.
/// If any breaking change would happen here, the old token would be removed and new unique token would be added.
///
/// Example:
/// ```c
/// CbcStagingSym symbols[] = {
///   { READ_WEAK_REF_TOKEN, (void*) &CJ_MCC_ReadWeakRef },
///   { "SomeOldToken", (void*) &CJ_MCC_NotNeededAnymore },
/// };
/// status = engine_initialize_rt_staging(symbols, sizeof(symbols) / sizeof(symbols[0]));
/// ```
CBC_EXPORT CbcStatus engine_initialize_rt_staging(CbcStagingSym* symbols, int count);

////////////////// Engine queries. //////////////////

/// A function in cbc can be represented as a pair of:
/// - `void* code;` function pointer - actual pointer to executable data;
/// - `void* fuh;` function handle - the pointer to the runtime representation of a function used for interpretation;
///
/// Note that both components of a pair can store `null` and at least one of them is `non-null`.
///
/// If `code != null` then the function can be called from AOT compiled managed code.
/// If `fuh != null` then the function can be called directly from the interpreter without performing ABI adaptations.
typedef struct CbcFunction {
    void* code;
    struct FunctionHandle* fuh;
} CbcFunction;

/// Query the function with the given name in the form of "<pkg/type name>::<func name>".
/// If `needTrampoline` is set, the `code` part of `CbcFunction` will contain a ready-to-use trampoline.
CBC_EXPORT CbcStatus engine_query_function(CbcFunction* rloc, char const* name, int needTrampoline);

/// The special version of `engine_query_function`, where the engine itself computes the actual `name` of the queried function.
CBC_EXPORT CbcStatus engine_query_main_function(CbcFunction* rloc);

////////////////// Library initialization stage. //////////////////

typedef struct CjBuiltins {
    CbcFunction implicitThrower;
    CbcFunction futureExecute;
    CbcFunction setRuntimeCJThreadHandle;
} CjBuiltins;

/// Initialize functionality and data provided by the std library for cbcengine to use.
CBC_EXPORT CbcStatus engine_lib_initialize_builtins(CjBuiltins* builtins);

////////////////// Post initialization. //////////////////

/// Install a validated hotfix set at a runtime-supported startup safe point.
CBC_EXPORT CbcStatus engine_apply_hotfix_patches();

////////////////// Convenience wrappers. //////////////////

/// Preset builtins initializer for cjnative AOT compiled stdlib.
CBC_EXPORT CbcStatus engine_preset_cjnative_builtins(CbcDlsym resolver);

/// Preset staging initialization that uses provided symbol resolver.
CBC_EXPORT CbcStatus engine_preset_init_staging(CbcDlsym resolver);

/// Preset resolver that performs symbol resolution in the current executable and its transitive dependencies.
CBC_EXPORT CbcStatus engine_preset_in_executable_resolver(CbcDlsym* rloc);

/// Preset resolver that performs symbol resolution in provided shared library handle.
CBC_EXPORT CbcStatus engine_preset_in_library_resolver(CbcDlsym* rloc, void *handle);

/// Preset resolver that performs symbol resolution in shared library that hosts provided symbol.
CBC_EXPORT CbcStatus engine_preset_in_host_library_resolver(CbcDlsym* rloc, void *symbolLocation);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CBC_ENGINE_H
