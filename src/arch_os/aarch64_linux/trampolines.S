#include "asm_export.h"

.macro declare_hidden sym
  .global \sym
  .hidden \sym
.endm

.extern engine_prepare_bytecode
.extern engine_interpretation_loop

// exported
declare_hidden engine_c2i_call_pc_start
declare_hidden engine_c2i_call_pc_end
declare_hidden engine_trampolines_dyn_start
declare_hidden engine_trampolines_direct_start
declare_hidden engine_iregs_only_c2i_call
declare_hidden engine_i2i_call
declare_hidden engine_i2c_call
declare_hidden engine_i2_newobject
declare_hidden engine_universal_direct_function_handles
declare_hidden engine_newobject_function


// --- SECTION BSS ---
.section .bss

// Fake Ectype* instance represented as global
// FIXME: get Ectype* instance from ThreadHandle
.align 16
.local engine_fake_ectype
engine_fake_ectype:
  .skip 1024

.align 16
engine_universal_direct_function_handles:
  .skip 8 * TRAMPOLINE_COUNT

.align 16
engine_newobject_function:
  .skip 8

// --- SECTION TEXT ---
.section .text

.align 16
engine_universal_direct_call:
  adrp  x10, engine_universal_direct_function_handles
  add   x10, x10, :lo12:engine_universal_direct_function_handles
  ldr   x9, [x10, x9, lsl #3] // put FuH on x9
  ldr   x10, [x9, #FUNCTION_HANDLE_C2CALL_OFFSET]
  br    x10


.align 16
engine_trampolines_direct_start:
.macro repeat_trampoline_direct count
  .set i, 0
  .rept \count
      mov x9, i
      b engine_universal_direct_call
      .set i, i + 1
  .endr
.endm
  repeat_trampoline_direct TRAMPOLINE_COUNT


.align 16
engine_universal_dyn_call:
  ldr   x10, [x0]
  ldr   x10, [x10, #TYPEINFO_DATA_MT_OFFSET] // TODO: consider tail array
  ldr   x9, [x10, x9, lsl #3] // put FuH on x9
  ldr   x10, [x9, #FUNCTION_HANDLE_C2CALL_OFFSET]
  br    x10


.align 16
engine_trampolines_dyn_start:
.macro repeat_trampoline_dyn count
  .set i, 0
  .rept \count
      mov x9, i
      b engine_universal_dyn_call
      .set i, i + 1
  .endr
.endm
  repeat_trampoline_dyn TRAMPOLINE_COUNT

#define TLS_REG x28
#define ECTYPE_REG x19
#define LITERALS_REG x20

.align 16
engine_c2i_call_pc_start:
engine_iregs_only_c2i_call:
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp

  stp   x19, x20, [sp, #-16]!
  stp   x21, x22, [sp, #-16]!
  stp   x23, x24, [sp, #-16]!
  stp   x27, x28, [sp, #-16]!

  str   x9, [sp, #-16]! // FunctionHandle that will help to scan saved regs

// FIXME: get Ectype* from ThreadHandle
//  mov   ECTYPE_REG, [TLS + TLS_FIBER_DATA_OFFSET]
//  mov   ECTYPE_REG, [ECTYPE_REG + FIBER_DATA_ECTYPE_OFFSET]
  adrp  ECTYPE_REG, engine_fake_ectype
  add   ECTYPE_REG, ECTYPE_REG, :lo12:engine_fake_ectype

// TODO support correct sret handling
  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8 )]
  str   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x10)]
  str   x2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x18)]
  str   x3, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x20)]
  str   x4, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x28)]
  str   x5, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x30)]
  str   x6, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x38)]
  str   x7, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x40)]

  mov   x0, x9
  bl    perform_2i_call

  ldr   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]

  add   sp, sp, #16 // remove FunctionHandle
  ldp   x27, x28, [sp], #16
  ldp   x23, x24, [sp], #16
  ldp   x21, x22, [sp], #16
  ldp   x19, x20, [sp], #16

  ldp   x29, x30, [sp], #16
  ret
engine_c2i_call_pc_end:
  brk   #0


.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2i_call:
  // jumped from .Linterpreter_call without return address to return
  bl    perform_2i_call
  b     .Lrestart_interpretation


.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2c_call:
  // jumped from .Linterpreter_call without return address to return
  mov   x9, x0

  ldr   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8 )]
  ldr   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x10)]
  ldr   x2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x18)]
  ldr   x3, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x20)]
  ldr   x4, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x28)]
  ldr   x5, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x30)]
  ldr   x6, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x38)]
  ldr   x7, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x40)]

  blr    x9

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]

  b     .Lrestart_interpretation


.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject:
  // jumped from .Linterpreter_call without return address to return
  adrp  x10, engine_newobject_function
  ldr   x10, [x10, :lo12:engine_newobject_function]
  blr   x10

  ldr   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 8)]
  add   x2, ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 8)
  str   x0, [x2, x1]
  b     .Lrestart_interpretation

.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: TLS_REG, ECTYPE_REG, x29
perform_2i_call:
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp
  // fp-0x8: FunctionHandle for frame scanning,
  // fp-0x10: Ectype
  // fp-0x18: Literals
  stp   x0, ECTYPE_REG, [sp, #-16]!
  str   LITERALS_REG, [sp, #-8]!

.set FRAME_BASE_SLOT, 0x10
.set SLOT_OFFS,   FRAME_BASE_SLOT
.set OFFS_ADDEND, 0x0

.macro declare_slot var, size
  .set OFFS_ADDEND, OFFS_ADDEND + \size
  .set \var, SLOT_OFFS + OFFS_ADDEND
.endm

#ifndef NDEBUG
  declare_slot READER_SLOT, 0x18
#else
  declare_slot READER_SLOT, 0x8
#endif

// align stack
  .if OFFS_ADDEND % 0x10 != 0
    .set OFFS_ADDEND, OFFS_ADDEND + 0x8
  .endif
  sub   sp, sp, OFFS_ADDEND
  // FIXME: perform stack check here!

  // We need to support fibers with movable stacks,
  // but to do it safely we need to adjust pointers to stack.
  // It is impossible to do correctly in C/C++ frames.
  // So, we will rely on "tail calls" from C++ interpreter loop
  // to managed compiled code.
  //
  // The problem is that we can't rely on tail call optimization
  // and can't force such behaviour reliably (early 2026).
  // Instead, we will return Thunk { function, arg } from C++ code to decide
  // how to continue execution further.

  ldr   x9, [x0, #FUNCTION_HANDLE_BYTECODE_OFFSET]
  cbnz  x9, .Lhave_bytecode
  // function handle is already on x0
  bl    engine_prepare_bytecode
  mov   x9, x0
.Lhave_bytecode:

  ldr   LITERALS_REG, [x9, #EXEC_BYTECODE_INFO_LITERALS_OFFSET]
  ldr   x1, [x9, #EXEC_BYTECODE_INFO_BYTECODE_OFFSET]
  sub   x10, x29, #READER_SLOT
#ifndef NDEBUG
  ldr   x8, [x9, #EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   x8, x8, x1
  str   x8, [x10, #0x10]
  str   x1, [x10, #0x8]
#endif
  str   x1, [x10]

  // current sp is 16-byte aligned
  ldr   w8, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  sub   sp, sp, x8

.Lrestart_interpretation:
  mov   x0, ECTYPE_REG
  mov   x1, sp
  mov   x2, TLS_REG
  mov   x3, LITERALS_REG
  sub   x4, x29, #READER_SLOT

  bl    engine_interpretation_loop

  // Thunk is returned on x0: function, x1: arg.
  // If usual return is needed, interpretation_loop will return null.
  cbz   x0, .Lend_interpretation

  mov   x9, x0
  mov   x0, x1
.Linterpreter_call:
  br    x9

.Lend_interpretation:
  mov   sp, x29
  ldp   x29, x30, [sp], #16
  ret
