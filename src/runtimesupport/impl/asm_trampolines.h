#pragma once

#include "RTInterface.h"
#include "RuntimeTypes.h"
#include "asm_export.h"

#include <cstdint>
#include <stddef.h>

/// Declarations of `trampolines.S` defined symbols.

namespace Asm {
extern "C" {

extern void engine_c2i_call_pc_start();
extern void engine_c2i_call_pc_end();
extern void engine_i2_newobject();
extern void engine_i2_newobject_acc();
extern void engine_i2_newarray();
extern void engine_i2_load_generic();
extern void engine_throw_implicit_exception();
extern void engine_handle_exception();
extern void engine_i2_gcpoint();
extern void engine_i2_spawn();
extern void engine_trampolines_direct_start();
extern void engine_trampolines_dyn_start();
extern void engine_trampolines_dyn_end();
extern void engine_trampolines_dyn_sret_start();
extern void engine_trampolines_dyn_sret_end();
extern void engine_iregs_only_c2i_call();
extern void engine_all_regs_c2i_call();
extern void engine_i2i_call();
extern void engine_i2c_call();
extern void common_landing_pad();
extern void* (*engine_tls_function)();
extern void (*engine_implicit_exception_thrower)(int kind);
extern void (*engine_throw_out_of_interpreter)(DYN_ObjRef exception);
extern void* (*engine_newobject_function)(DYN_TypeInfo*);
extern void* (*engine_newthread_nret_function)(void*, DYN_ObjRef, void*, DYN_TypeInfo*);
extern void* (*engine_newarray_function)(DYN_TypeInfo*, uint64_t);
extern void* engine_universal_direct_function_handles[TRAMPOLINE_COUNT];

extern DYN_ReadGenericFieldFn engine_read_generic;

extern size_t engine_carrier_specific_offset;
extern size_t engine_cjthread_specific_offset;

} // extern "C"
} // namespace Asm
