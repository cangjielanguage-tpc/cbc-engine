#pragma once

#include "RuntimeTypes.h"
#include "asm_export.h"

#include <stddef.h>

/// Declarations of `trampolines.S` defined symbols.

namespace Asm {
extern "C" {

extern void engine_c2i_call_pc_start();
extern void engine_c2i_call_pc_end();
extern void engine_i2_newobject();
extern void engine_i2_gcpoint();
extern void engine_trampolines_direct_start();
extern void engine_trampolines_dyn_start();
extern void engine_iregs_only_c2i_call();
extern void engine_i2i_call();
extern void engine_i2c_call();
extern void* (*engine_newobject_function)(DYN_TypeInfoT*);
extern void* engine_universal_direct_function_handles[TRAMPOLINE_COUNT];

extern size_t engine_carrier_specific_offset;
extern size_t engine_cjthread_specific_offset;

} // extern "C"
} // namespace Asm
