#ifndef CBC_ENGINE_H
#define CBC_ENGINE_H

#define CBC_EXPORT __attribute__((visibility("default"), used))

/// This file define C-API exposed to the cbc engine library users.

#ifdef __cplusplus
extern "C" {
#endif

/// Return main function trampoline with cjnative calling convention.
CBC_EXPORT void* engine_get_entrypoint_trampoline(void);

/// Set the folder where cbc libraries contained.
/// The paths should be separated with `:` character.
///
/// The string passed is expected to live as long, as engine lives.
CBC_EXPORT void engine_set_cbcpath(char const* cbc_path);

/// Prepare for interpretation.
/// Load provided main cbc and open needed AOT dynamic libraries.
CBC_EXPORT void engine_initialize();

/// Set the path to main cbc that contains `main` function.
///
/// The string passed is expected to live as long, as engine lives.
CBC_EXPORT void engine_set_main_cbc(char const* main_cbc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // CBC_ENGINE_H
