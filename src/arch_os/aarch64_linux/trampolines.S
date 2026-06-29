#include "asm_export.h"
#include "abi.inc"

.macro declare_hidden sym
  .global \sym
  .hidden \sym
.endm

.extern engine_prepare_bytecode
.extern engine_interpretation_loop
.extern engine_fiber_data_init

// exported
declare_hidden engine_c2i_call_pc_start
declare_hidden engine_c2i_call_pc_end
declare_hidden engine_trampolines_dyn_start
declare_hidden engine_trampolines_dyn_end
declare_hidden engine_trampolines_direct_start
declare_hidden engine_all_regs_c2i_call
declare_hidden engine_iregs_only_c2i_call
declare_hidden engine_i2i_call
declare_hidden engine_i2c_call
declare_hidden engine_i2_newobject
declare_hidden engine_i2_newobject_acc
declare_hidden engine_i2_newarray
declare_hidden engine_i2_gcpoint
declare_hidden engine_universal_direct_function_handles
declare_hidden engine_tls_function
declare_hidden engine_read_generic
declare_hidden engine_i2_load_generic
declare_hidden engine_newobject_function
declare_hidden engine_newarray_function
declare_hidden engine_newthread_nret_function
declare_hidden engine_carrier_specific_offset
declare_hidden engine_cjthread_specific_offset
declare_hidden engine_i2_spawn

// --- SECTION BSS ---
.section .bss

.balign 16
engine_universal_direct_function_handles:
  .skip 8 * TRAMPOLINE_COUNT

.balign 64
.Lfunc_anchor:
engine_tls_function:
  .skip 8
engine_newobject_function:
  .skip 8
engine_newarray_function:
  .skip 8
engine_newthread_nret_function:
  .skip 8
engine_read_generic:
  .skip 8
engine_carrier_specific_offset:
  .skip 8
engine_cjthread_specific_offset:
  .skip 8

// --- SECTION TEXT ---
.section .text

.balign 16
engine_universal_direct_call:
  adrp  x10, engine_universal_direct_function_handles
  add   x10, x10, :lo12:engine_universal_direct_function_handles
  ldr   x9, [x10, x9, lsl #3] // put FuH on x9
  ldr   x10, [x9, #FUNCTION_HANDLE_C2CALL_OFFSET]
  br    x10


.balign 16
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


.balign 16
engine_universal_dyn_call:
  ldr   x10, [x0]
  ldr   x10, [x10, #TYPEINFO_DATA_MT_OFFSET] // TODO: consider tail array
  ldr   x9, [x10, x9, lsl #3] // put FuH on x9
  ldr   x10, [x9, #FUNCTION_HANDLE_C2CALL_OFFSET]
  br    x10


.balign 16
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
engine_trampolines_dyn_end:

.macro engine_c2i_call ireg_count freg_count
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp

  stp   x19, x20, [sp, #-16]! // x29 - 0x10
  stp   x21, x22, [sp, #-16]! // x29 - 0x20
  stp   x23, x24, [sp, #-16]! // x29 - 0x30
  stp   x27, x28, [sp, #-16]! // x29 - 0x40

  str   x9, [sp, #-16]! // x29 - 0x50 : FunctionHandle that will help to scan saved regs

  adrp  ECTYPE_REG, engine_carrier_specific_offset
  ldr   ECTYPE_REG, [ECTYPE_REG, :lo12:engine_carrier_specific_offset]
  ldr   x21, [TLS_REG, ECTYPE_REG]
  adrp  ECTYPE_REG, engine_cjthread_specific_offset
  ldr   ECTYPE_REG, [ECTYPE_REG, :lo12:engine_cjthread_specific_offset]
  add   x21, x21, ECTYPE_REG // address of slot in fiber data that contains ectype addr
  ldr   ECTYPE_REG, [x21]    // ectype addr

  cbz   ECTYPE_REG, .Lc2i_prepare_ectype\@
.Lc2i_args_preparation\@:
  // TODO support correct sret handling

  c2i_emit_iregs_abi \ireg_count
  c2i_emit_fregs_abi \freg_count

  mov   x0, x9
  bl    perform_2i_call

  ldr   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]
  ldr   d0, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET)]

  add   sp, sp, #16 // remove FunctionHandle
  ldp   x27, x28, [sp], #16
  ldp   x23, x24, [sp], #16
  ldp   x21, x22, [sp], #16
  ldp   x19, x20, [sp], #16

  ldp   x29, x30, [sp], #16
  ret

.Lc2i_prepare_ectype\@:
  bl c2i_prepare_ectype
  b     .Lc2i_args_preparation\@
.endm

.balign 16
c2i_prepare_ectype:
  stp   x29, x30, [sp, #-16]!
  stp   x0, x1, [sp, #-16]!
  stp   x2, x3, [sp, #-16]!
  stp   x4, x5, [sp, #-16]!
  stp   x6, x7, [sp, #-16]!

  mov   x0, x21
  bl    engine_fiber_data_init

  mov   ECTYPE_REG, x0
  ldr   x9, [x29, #-0x50] // restore fuh

  ldp   x6, x7, [sp], #16
  ldp   x4, x5, [sp], #16
  ldp   x2, x3, [sp], #16
  ldp   x0, x1, [sp], #16
  ldp   x29, x30, [sp], #16
  ret


.balign 16
// label that has to cover all c2i calls to range-check them
engine_c2i_call_pc_start:
engine_iregs_only_c2i_call:
  engine_c2i_call IREG_ABI_AMOUNT 0

.balign 16
engine_all_regs_c2i_call:
  engine_c2i_call IREG_ABI_AMOUNT FREG_ABI_AMOUNT

engine_c2i_call_pc_end:
  brk   #0


.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2i_call:
  // jumped from .Linterpreter_call without return address to return
  bl    perform_2i_call
  b     .Lrestart_interpretation

// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: target)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
.macro engine_i2c ireg_count freg_count
  // jumped from .Linterpreter_call without return address to return
  mov   x9, x0

  i2c_emit_iregs_abi \ireg_count
  i2c_emit_fregs_abi \freg_count

  blr   x9

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]

  b     .Lrestart_interpretation
.endm

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: target)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2c_call:
  engine_i2c IREG_ABI_AMOUNT FREG_ABI_AMOUNT


.balign 16
get_tls:
  adrp  x10, engine_tls_function
  ldr   x10, [x10, :lo12:engine_tls_function]
  br    x10

.macro restore_tls // spoils volatile registers!
  bl    get_tls
  mov   TLS_REG, x0
.endm

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: mask)
// preserved: ECTYPE_REG, rbp
engine_i2_load_generic:
  // mask is expected in form of:
  // [0..4) - dst reg
  // [4..8) - base reg
  // [8..12) - field reg
  // [12..64) - typeInfo

  mov   EXTRA_REG, x0
  // allocate box
  adrp  x4, engine_newobject_function
  ldr   x4, [x4, :lo12:engine_newobject_function]
  ubfx  x0, x0, 12, 52
  blr   x4
  // box is already on dst param of read generic

  add   x7, ECTYPE_REG, ECTYPE_IREGS_OFFSET

  // save box to dst reg
  and   w1, EXTRA_REGd, 0xf
  str   x0, [x7, x1, lsl 3]

  // obj (base) param
  ubfx  w1, EXTRA_REGd, 4, 4
  ldr   x1, [x7, x1, lsl 3]

  // field param
  ubfx  w2, EXTRA_REGd, 8, 4
  ldr   x2, [x7, x2, lsl 3]

  // size param
  ldr   x3, [x0] // get header
  ldr   x3, [x3, #TYPEINFO_INSTANCESIZE_OFFSET]
  // perform generic read
  adrp  x4, engine_read_generic
  ldr   x4, [x4, :lo12:engine_read_generic]
  blr   x4
  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject:
  // jumped from .Linterpreter_call without return address to return
  adrp  x10, engine_newobject_function
  ldr   x10, [x10, :lo12:engine_newobject_function]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject_acc:
  // jumped from .Linterpreter_call without return address to return
  adrp  x10, engine_newobject_function
  ldr   x10, [x10, :lo12:engine_newobject_function]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8 * 14)]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newarray:
  // jumped from .Linterpreter_call without return address to return
  ldr   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x10)]
  adrp  x10, engine_newarray_function
  ldr   x10, [x10, :lo12:engine_newarray_function]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
engine_i2_gcpoint:
  blr   x0
  restore_tls
  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
engine_i2_spawn:
  ldr   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]
  mov   x3, x0
  adr   x0, engine_fake_execute_closure
  mov   x2, 0

  adrp  x4, engine_newthread_nret_function
  ldr   x4, [x4, :lo12:engine_newthread_nret_function]
  blr   x4
  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: TLS_REG, ECTYPE_REG, x29
perform_2i_call:
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp
  // fp-0x8: FunctionHandle for frame scanning,
  // fp-0x10: Ectype
  // fp-0x18: Literals
  // fp-0x20: Extra saved reg
  sub   sp, sp, LOCAL_SLOTS_OFFSET
  stp   ECTYPE_REG, x0,           [x29, #-ECTYPE_REG_SAVED_SLOT_OFFSET]
  stp   EXTRA_REG,  LITERALS_REG, [x29, #-EXTRA_SAVED_REG_OFFSET]
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
  sub   x10, x29, #READER_SLOT_OFFSET
#ifndef NDEBUG
  ldr   x8, [x9, #EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   x8, x8, x1
  str   x8, [x29, #0x10]
  str   x1, [x29, #0x8]
#endif
  str   x1, [x10]

  mov   x1, sp // save current sp for registers saving to the bottom of frame

  // current sp is 16-byte.baligned
  ldr   w8, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  sub   sp, sp, x8

.macro PUSH_IREGS mask_reg, stack_reg, exec_info_reg
    ldrh  \mask_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    .irp reg, 10,11,12,13
        tbz \mask_reg, #(\reg-8), .Lsaveireg\reg
        ldr x2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \reg*ECTYPE_REG_SIZE)]
        str x2, [\stack_reg, #-ECTYPE_REG_SIZE]!
.Lsaveireg\reg:
    .endr
.endm

.macro PUSH_FREGS mask_reg, stack_reg, exec_info_reg
    ldrh  \mask_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13,14,15
        tbz \mask_reg, #(\reg-8), .Lsavefreg\reg
        ldr d2, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \reg*ECTYPE_REG_SIZE)]
        str d2, [\stack_reg, #-ECTYPE_REG_SIZE]!
.Lsavefreg\reg:
    .endr
.endm

  PUSH_IREGS w8, x1, x9
  PUSH_FREGS w8, x1, x9

.Lrestart_interpretation:
  mov   x0, ECTYPE_REG
  mov   x1, sp
  mov   x2, TLS_REG
  mov   x3, LITERALS_REG
  sub   x4, x29, #READER_SLOT_OFFSET

  bl    engine_interpretation_loop

  // Thunk is returned on x0: function, x1: arg.
  // If usual return is needed, interpretation_loop will return null.
  cbz   x0, .Lend_interpretation

  mov   x9, x0
  mov   x0, x1
.Linterpreter_call:
  br    x9

.Lend_interpretation:

  ldr   x9, [x29, #-0x8] // load Function Handle
  ldr   x9, [x9, #FUNCTION_HANDLE_BYTECODE_OFFSET]

  ldr   w1, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  mov   x2, sp
  add   x1, x1, x2 // add frame size to locate saved registers

.macro POP_IREGS mask_reg, stack_reg, exec_info_reg
    ldrh  \mask_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    .irp reg, 10,11,12,13
        tbz \mask_reg, #(\reg-8), .Lloadireg\reg
        ldr x2, [\stack_reg, #-0x8]!
        str x2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \reg*8)]
.Lloadireg\reg:
    .endr
.endm

.macro POP_FREGS mask_reg, stack_reg, exec_info_reg
    ldrh  \mask_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13,14,15
        tbz \mask_reg, #(\reg-8), .Lloadfreg\reg
        ldr d2, [\stack_reg, #-0x8]!
        str d2, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \reg*8)]
.Lloadfreg\reg:
    .endr
.endm

  POP_IREGS w8, x1, x9
  POP_FREGS w8, x1, x9

  .if (LITERALS_REG_SAVED_SLOT_OFFSET + 0x8 != EXTRA_SAVED_REG_OFFSET)
    .error "offsets are not consecutive"
  .endif
  ldp   LITERALS_REG, EXTRA_REG, [x29, #-EXTRA_SAVED_REG_OFFSET]
  ldr   ECTYPE_REG, [x29, #-ECTYPE_REG_SAVED_SLOT_OFFSET]

  mov   sp, x29
  ldp   x29, x30, [sp], #16
  ret

// fake execute closure function
.balign 16
engine_fake_execute_closure:
  // (x8: sret, x0: ()T, x1: TypeInfo[Future[T]]
  ldr   x2, [x0, #0x10]
  br    x2
