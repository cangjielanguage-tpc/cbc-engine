#include "asm_export.h"
#include "abi.inc"

#ifdef __APPLE__
    #define engine_prepare_bytecode _engine_prepare_bytecode
    #define engine_interpretation_loop _engine_interpretation_loop
    #define engine_fiber_data_init _engine_fiber_data_init
    #define engine_get_exception_handler _engine_get_exception_handler
    #define engine_get_and_clear_pending_exception _engine_get_and_clear_pending_exception
    #define engine_c2i_call_pc_start _engine_c2i_call_pc_start
    #define engine_c2i_call_pc_end _engine_c2i_call_pc_end
    #define engine_trampolines_dyn_start _engine_trampolines_dyn_start
    #define engine_trampolines_dyn_end _engine_trampolines_dyn_end
    #define engine_trampolines_dyn_sret_start _engine_trampolines_dyn_sret_start
    #define engine_trampolines_dyn_sret_end _engine_trampolines_dyn_sret_end
    #define engine_trampolines_direct_start _engine_trampolines_direct_start
    #define engine_all_regs_c2i_call _engine_all_regs_c2i_call
    #define engine_iregs_only_c2i_call _engine_iregs_only_c2i_call
    #define engine_i2i_call _engine_i2i_call
    #define engine_i2c_call _engine_i2c_call
    #define engine_i2_newobject _engine_i2_newobject
    #define engine_i2_newobject_acc _engine_i2_newobject_acc
    #define engine_i2_newobject_pinned _engine_i2_newobject_pinned
    #define engine_newobject_pinned_function _engine_newobject_pinned_function
    #define engine_spawn_future _engine_spawn_future
    #define engine_future_execute_function _engine_future_execute_function
    #define engine_set_runtime_cjthread_handle_function _engine_set_runtime_cjthread_handle_function
    #define engine_i2_spawn_future _engine_i2_spawn_future
    #define engine_i2_newarray _engine_i2_newarray
    #define engine_i2_load_generic _engine_i2_load_generic
    #define engine_i2_gcpoint _engine_i2_gcpoint
    #define engine_i2_spawn _engine_i2_spawn
    #define engine_universal_direct_function_handles _engine_universal_direct_function_handles
    #define engine_tls_function _engine_tls_function
    #define engine_read_generic _engine_read_generic
    #define engine_newobject_function _engine_newobject_function
    #define engine_newarray_function _engine_newarray_function
    #define engine_newthread_nret_function _engine_newthread_nret_function
    #define engine_carrier_specific_offset _engine_carrier_specific_offset
    #define engine_cjthread_specific_offset _engine_cjthread_specific_offset
    #define engine_throw_out_of_interpreter _engine_throw_out_of_interpreter
    #define engine_throw_implicit_exception _engine_throw_implicit_exception
    #define engine_implicit_exception_thrower _engine_implicit_exception_thrower
    #define engine_handle_exception _engine_handle_exception
    #define common_landing_pad _common_landing_pad
    #define engine_stack_grow_stub _engine_stack_grow_stub
    #define engine_after_stack_grow _engine_after_stack_grow
#endif

#ifdef __APPLE__
    #define Page(sym) sym@PAGE
    #define PageOff(sym) sym@PAGEOFF
#else
    #define Page(sym) sym
    #define PageOff(sym) :lo12:sym
#endif

.macro declare_hidden sym
  .global \sym
#ifdef __APPLE__
  .private_extern \sym
#else
  .hidden \sym
#endif
.endm

.extern engine_prepare_bytecode
.extern engine_interpretation_loop
.extern engine_fiber_data_init
.extern engine_get_exception_handler
.extern engine_get_and_clear_pending_exception
.extern engine_spawn_future

// exported
declare_hidden engine_c2i_call_pc_start
declare_hidden engine_c2i_call_pc_end
declare_hidden engine_trampolines_dyn_start
declare_hidden engine_trampolines_dyn_end
declare_hidden engine_trampolines_dyn_sret_start
declare_hidden engine_trampolines_dyn_sret_end
declare_hidden engine_trampolines_direct_start
declare_hidden engine_all_regs_c2i_call
declare_hidden engine_iregs_only_c2i_call
declare_hidden engine_i2i_call
declare_hidden engine_i2c_call
declare_hidden engine_i2_newobject_pinned
declare_hidden engine_i2_newobject
declare_hidden engine_i2_newobject_acc
declare_hidden engine_i2_newarray
declare_hidden engine_i2_gcpoint
declare_hidden engine_universal_direct_function_handles
declare_hidden engine_tls_function
declare_hidden engine_read_generic
declare_hidden engine_i2_load_generic
declare_hidden engine_newobject_function
declare_hidden engine_newobject_pinned_function
declare_hidden engine_newarray_function
declare_hidden engine_newthread_nret_function
declare_hidden engine_carrier_specific_offset
declare_hidden engine_cjthread_specific_offset
declare_hidden engine_i2_spawn
declare_hidden engine_future_execute_function
declare_hidden engine_set_runtime_cjthread_handle_function
declare_hidden engine_i2_spawn_future
declare_hidden engine_throw_out_of_interpreter
declare_hidden engine_throw_implicit_exception
declare_hidden engine_implicit_exception_thrower
declare_hidden engine_handle_exception
declare_hidden common_landing_pad
declare_hidden engine_stack_grow_stub
declare_hidden engine_after_stack_grow

// --- SECTION BSS ---
#ifdef __APPLE__
.section __DATA,__bss
#else
.section .bss
#endif

.balign 16
engine_universal_direct_function_handles:
  .skip 8 * TRAMPOLINE_COUNT

.balign 64
.Lfunc_anchor:
engine_tls_function:
  .skip 8
engine_throw_out_of_interpreter:
  .skip 8
engine_implicit_exception_thrower:
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
engine_stack_grow_stub:
  .skip 8
engine_future_execute_function:
  .skip 8
engine_set_runtime_cjthread_handle_function:
  .skip 8
engine_newobject_pinned_function:
  .skip 8

// --- SECTION TEXT ---
#ifdef __APPLE__
.section __TEXT,__text,regular,pure_instructions
#else
.section .text
#endif

.macro PUSH_REGS list_reg, stack_reg, exec_info_reg, tmp1, tmp2
    ldr   \list_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    cbz   \list_reg, .L\@iend
    add   \tmp2, ECTYPE_REG, ECTYPE_IREGS_OFFSET
.L\@icopy:
    and   \tmp1, \list_reg, 15
    lsr   \list_reg, \list_reg, 4
    ldr   \tmp1, [\tmp2, \tmp1, lsl #3]
    str   \tmp1, [\stack_reg, #-ECTYPE_REG_SIZE]!
    cbnz  \list_reg, .L\@icopy
.L\@iend:
    ldr   \list_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    cbz   \list_reg, .L\@fend
    add   \tmp2, ECTYPE_REG, ECTYPE_FREGS_OFFSET
.L\@fcopy:
    and   \tmp1, \list_reg, 15
    lsr   \list_reg, \list_reg, 4
    ldr   \tmp1, [\tmp2, \tmp1, lsl #3]
    str   \tmp1, [\stack_reg, #-ECTYPE_REG_SIZE]!
    cbnz  \list_reg, .L\@fcopy
.L\@fend:
.endm

.macro POP_REGS list_reg, stack_reg, exec_info_reg, tmp1, tmp2, tmp3
    ldr   \list_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    cbz   \list_reg, .L\@iend
    add   \tmp2, ECTYPE_REG, ECTYPE_IREGS_OFFSET
.L\@icopy:
    ldr   \tmp3, [\stack_reg, #-ECTYPE_REG_SIZE]!
    and   \tmp1, \list_reg, 15
    lsr   \list_reg, \list_reg, 4
    str   \tmp3, [\tmp2, \tmp1, lsl #3]
    cbnz  \list_reg, .L\@icopy
.L\@iend:
    ldr   \list_reg, [\exec_info_reg, #EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    cbz   \list_reg, .L\@fend
    add   \tmp2, ECTYPE_REG, ECTYPE_FREGS_OFFSET
.L\@fcopy:
    ldr   \tmp3, [\stack_reg, #-ECTYPE_REG_SIZE]!
    and   \tmp1, \list_reg, 15
    lsr   \list_reg, \list_reg, 4
    str   \tmp3, [\tmp2, \tmp1, lsl #3]
    cbnz  \list_reg, .L\@fcopy
.L\@fend:
.endm

.balign 16
engine_universal_direct_call:
  adrp  x10, Page(engine_universal_direct_function_handles)
  add   x10, x10, PageOff(engine_universal_direct_function_handles)
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
// AArch64 passes the SRET pointer in x8, leaving the receiver in x0. We can use same trampoline for both SRET and non-SRET.
engine_trampolines_dyn_sret_start:
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
engine_trampolines_dyn_sret_end:
engine_trampolines_dyn_end:

#define StubFrameContextSize   (8 * 14)
#define StubCalleeSaveAreaSize (8 * 4)

.macro engine_c2i_call ireg_count freg_count
  mov   x12, sp
  stp   x29, x30, [sp, #-StubFrameContextSize]!
  mov   x29, sp

  stp   x19, x20, [sp, #StubCalleeSaveAreaSize]
  stp   x21, x22, [sp, #StubCalleeSaveAreaSize+0x10]
  stp   x23, x24, [sp, #StubCalleeSaveAreaSize+0x20]
  stp   x25, x26, [sp, #StubCalleeSaveAreaSize+0x30]
  stp   x27, x28, [sp, #StubCalleeSaveAreaSize+0x40]

  mov   x27, x12 // previous stack top
  mov   x26, x9  // save fuh

  adrp  ECTYPE_REG, Page(engine_carrier_specific_offset)
  ldr   ECTYPE_REG, [ECTYPE_REG, PageOff(engine_carrier_specific_offset)]
  ldr   x21, [TLS_REG, ECTYPE_REG]
  adrp  ECTYPE_REG, Page(engine_cjthread_specific_offset)
  ldr   ECTYPE_REG, [ECTYPE_REG, PageOff(engine_cjthread_specific_offset)]
  add   x21, x21, ECTYPE_REG // address of slot in fiber data that contains ectype addr
  ldr   ECTYPE_REG, [x21]    // ectype addr

  cbz   ECTYPE_REG, .Lc2i_prepare_ectype\@
.Lc2i_args_preparation\@:

  c2i_emit_iregs_abi \ireg_count
  c2i_emit_fregs_abi \freg_count

  str   x27, [ECTYPE_REG, #TAIL_REG_OFFS]
  mov   x0, x26
  bl    perform_2i_call

  ldr   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x8)]
  ldr   d0, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET)]

  ldp   x19, x20, [sp, #StubCalleeSaveAreaSize]
  ldp   x21, x22, [sp, #StubCalleeSaveAreaSize+0x10]
  ldp   x23, x24, [sp, #StubCalleeSaveAreaSize+0x20]
  ldp   x25, x26, [sp, #StubCalleeSaveAreaSize+0x30]
  ldr   x27,      [sp, #StubCalleeSaveAreaSize+0x40] // do not restore tls reg
  ldp   x29, x30, [sp], #StubFrameContextSize
  ret

.Lc2i_prepare_ectype\@:
  bl c2i_prepare_ectype
  b     .Lc2i_args_preparation\@
.endm

.balign 16
c2i_prepare_ectype:
  // param passing registers could store real parameters of called c2i method
  stp   x29, x30, [sp, #-16]!
  stp   x0, x1, [sp, #-16]!
  stp   x2, x3, [sp, #-16]!
  stp   x4, x5, [sp, #-16]!
  stp   x6, x7, [sp, #-16]!
  str   x8,     [sp, #-16]!

  stp   d0, d1, [sp, #-16]!
  stp   d2, d3, [sp, #-16]!
  stp   d4, d5, [sp, #-16]!
  stp   d6, d7, [sp, #-16]!

  mov   x0, x21
  bl    engine_fiber_data_init

  mov   ECTYPE_REG, x0

  ldp   d6, d7, [sp], #16
  ldp   d4, d5, [sp], #16
  ldp   d2, d3, [sp], #16
  ldp   d0, d1, [sp], #16

  ldr   x8,     [sp], #16
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
  engine_c2i_call IREG_ABI_AMOUNT, 0

.balign 16
engine_all_regs_c2i_call:
  engine_c2i_call IREG_ABI_AMOUNT, FREG_ABI_AMOUNT

engine_c2i_call_pc_end:
  brk   #0

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: TLS_REG, ECTYPE_REG, x29
perform_2i_call:
// The final layout of the frame is following:
// +------------------------------------+
// | Return Address                     |
// +------------------------------------+
// | Saved Frame Pointer (Old FP)       |
// +------------------------------------+>\ <-- FP
// | FuH (x0)                           |  \
// +------------------------------------+  |
// | ECTYPE_REG                         |  |
// +------------------------------------+  |
// | LITERALS_REG                       |  |
// +------------------------------------+  | -- LOCAL_SLOTS_OFFSET
// | EXTRA_REG                          |  |
// +------------------------------------+  |
// | CHECK_SUM                          |  |
// +------------------------------------+  |
// | Reader                             |  |
// +------------------------------------+  |
// | Alignment  (0 or 8)                |  /
// +------------------------------------+>/
// | Dynamic Locals (locals_size)       |
// +------------------------------------+ <-- SP
  stp   x29, x30, [sp, #-16]!
  mov   x29, sp
  // fp-0x8: FunctionHandle for frame scanning,
  // fp-0x10: Ectype
  // fp-0x18: Literals
  // fp-0x20: Extra saved reg
  // fp-0x28: Check sum (needed for debugging) TODO: support it for aarch64
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
#ifndef NDEBUG
  ldr   x8, [x9, #EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   x8, x8, x1
  str   x8, [x29, #-READER_SLOT_OFFSET + 0x10]
  str   x1, [x29, #-READER_SLOT_OFFSET + 0x8]
#endif
  str   x1, [x29, #-READER_SLOT_OFFSET]

#ifndef NDEBUG
  ldr   w1, [ECTYPE_REG, #ECTYPE_FUNC_COUNTER_OFFSET]
  add   w1, w1, #1
  str   w1, [ECTYPE_REG, #ECTYPE_FUNC_COUNTER_OFFSET]
  ldr   x0, [x29, #-FUH_SLOT_OFFSET]
  mov   x1, ECTYPE_REG
  bl    engine_log_int_start

  ldr   x9, [x29, #-FUH_SLOT_OFFSET]
  ldr   x9, [x9, #FUNCTION_HANDLE_BYTECODE_OFFSET]
#endif

.Lcontinue_after_stack_expand:
  mov   x1, sp // save current sp for registers saving to the bottom of frame

  // current sp is 16-byte.baligned
  ldr   w8, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  sub   sp, sp, x8

  ldr   x10, [TLS_REG, #TLS_STACK_BORDER_OFFSET]
  add   x10, x10, #ADDITIONAL_STACK_SPACE
  cmp   sp, x10
  b.ls  .Lstack_overflow

  mov   x2, sp
  mov   x3, x1

// FIXME: remove it
// We are zeroing a whole frame to simplify gc handling,
// but it should be handled by compiler properly.
.Lframe_zero_loop:
  cmp   x2, x3
  b.eq   .Lframe_zero_loop_end
  str   xzr, [x2], #8
  b     .Lframe_zero_loop
.Lframe_zero_loop_end:

  PUSH_REGS x8, x1, x9, x2, x3

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

#ifndef NDEBUG
  ldr   x0, [x29, #-FUH_SLOT_OFFSET]
  mov   x1, ECTYPE_REG
  bl    engine_log_int_end
#endif

  ldr   x9, [x29, #-FUH_SLOT_OFFSET] // load Function Handle
  ldr   x9, [x9, #FUNCTION_HANDLE_BYTECODE_OFFSET]

  ldr   w1, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  mov   x2, sp
  add   x1, x1, x2 // add frame size to locate saved registers

  POP_REGS x8, x1, x9, x2, x3, x4

  ldr   ECTYPE_REG, [x29, #-ECTYPE_REG_SAVED_SLOT_OFFSET]
  ldp   EXTRA_REG, LITERALS_REG, [x29, #-EXTRA_SAVED_REG_OFFSET]

  mov   sp, x29
  ldp   x29, x30, [sp], #16
  ret

.macro save_all_iregs scratch_reg
  .set _offset, 0
  .rept ECTYPE_IREGS_COUNT
    ldr   \scratch_reg, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + _offset)]
    str   \scratch_reg, [sp, #_offset]
    .set _offset, _offset + ECTYPE_REG_SIZE
  .endr
.endm

.macro restore_all_iregs scratch_reg
  .set _offset, 0
  .rept ECTYPE_IREGS_COUNT
    ldr   \scratch_reg, [sp, #_offset]
    str   \scratch_reg, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + _offset)]
    .set _offset, _offset + ECTYPE_REG_SIZE
  .endr
.endm

// Expands stack and returns to perform 2i call or throws SOE if stack expansion is impossible
.balign 16
.Lstack_overflow:
  add   sp, sp, x8

  sub   sp, sp, #STACK_OVERFLOW_REGDUMP_SIZE
  mov   x10, sp
  str   x10, [ECTYPE_REG, #ECTYPE_IACC_OFFSET]
  save_all_iregs x10

  mov   x9, #0x20 // adjusted size
  adrp  x10, Page(engine_stack_grow_stub)
  ldr   x10, [x10, PageOff(engine_stack_grow_stub)]
  blr   x10
engine_after_stack_grow:

  restore_all_iregs x10
  add   sp, sp, #STACK_OVERFLOW_REGDUMP_SIZE

  // Restore ExecBytecodeInfo
  ldr   x9, [x29, #-FUH_SLOT_OFFSET]
  ldr   x9, [x9, #FUNCTION_HANDLE_BYTECODE_OFFSET]

  b     .Lcontinue_after_stack_expand

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: fuh)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2i_call:
  // stack parameters are starting right at the top of the interpreted frame
  mov   x1, sp
  str   x1, [ECTYPE_REG, #TAIL_REG_OFFS]

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

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE)]
  str   d0, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET)]

  b     .Lrestart_interpretation
.endm

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: target)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2c_call:
  engine_i2c IREG_ABI_AMOUNT, FREG_ABI_AMOUNT


.balign 16
get_tls:
  adrp  x10, Page(engine_tls_function)
  ldr   x10, [x10, PageOff(engine_tls_function)]
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
  adrp  x4, Page(engine_newobject_function)
  ldr   x4, [x4, PageOff(engine_newobject_function)]
  lsr   x0, x0, 12
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
  ldr   w3, [x3, #TYPEINFO_INSTANCESIZE_OFFSET]
  // perform generic read
  adrp  x4, Page(engine_read_generic)
  ldr   x4, [x4, PageOff(engine_read_generic)]
  blr   x4
  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject:
  // jumped from .Linterpreter_call without return address to return
  adrp  x10, Page(engine_newobject_function)
  ldr   x10, [x10, PageOff(engine_newobject_function)]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE)]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject_pinned:
  // jumped from .Linterpreter_call without return address to return
  mov   x1, 0
  adrp  x10, Page(engine_newobject_pinned_function)
  ldr   x10, [x10, PageOff(engine_newobject_pinned_function)]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE)]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newobject_acc:
  // jumped from .Linterpreter_call without return address to return
  adrp  x10, Page(engine_newobject_function)
  ldr   x10, [x10, PageOff(engine_newobject_function)]
  blr   x10

  str   x0, [ECTYPE_REG, #ECTYPE_IACC_OFFSET]
  restore_tls

  b     .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: typeInfo)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_i2_newarray:
  // jumped from .Linterpreter_call without return address to return
  ldr   x1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + 0x10)]
  adrp  x10, Page(engine_newarray_function)
  ldr   x10, [x10, PageOff(engine_newarray_function)]
  blr   x10

  str   x0, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE)]
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
  ldr   x1, [ECTYPE_REG, #ECTYPE_IACC_OFFSET]
  mov   x3, x0
  adr   x0, engine_fake_execute_closure
  mov   x2, 0

  adrp  x4, Page(engine_newthread_nret_function)
  ldr   x4, [x4, PageOff(engine_newthread_nret_function)]
  blr   x4
  b     .Lrestart_interpretation

// expects: (ECTYPE_REG: ectype, TLS_REG: th)
// preserved: ECTYPE_REG, x29
engine_i2_spawn_future:
  ldr   x0, [ECTYPE_REG, #ECTYPE_IACC_OFFSET]
  bl    engine_spawn_future
  cbnz  x1, .Lspawn_future_success
  mov   x0, IMPLICIT_EXCEPTION_SPAWN
  b     engine_throw_implicit_exception
.Lspawn_future_success:
  // Thread.setRuntimeCJThreadHandle arguments:
  // x8: sret pointer for Unit; no return storage is needed.
  // x0: this (Thread object already returned by engine_spawn_future in x0).
  // x1: handle (CJThreadHandle already returned by engine_spawn_future in x1).
  // x2: outerTI (unused by this method, so pass null).
  mov   x8, sp
  mov   x2, xzr
  adrp  x9, Page(engine_set_runtime_cjthread_handle_function)
  ldr   x9, [x9, PageOff(engine_set_runtime_cjthread_handle_function)]
  blr   x9
  b     .Lrestart_interpretation

// TREAT SYMBOLS ABOVE AS PART OF perform_2i_call
#ifdef __ELF__
.size perform_2i_call, .-perform_2i_call
#endif

// fake execute closure function
.balign 16
engine_fake_execute_closure:
  // (x8: sret, x0: ()T, x1: TypeInfo[Future[T]]
  ldr   x2, [x0, #0x10]
  br    x2

// expects: (ECTYPE_REG: ectype, TLS_REG: th, x0: exception type)
// preserved: ECTYPE_REG, x29
// TLS_REG can be changed after context-switch
engine_throw_implicit_exception:
  // jumped from .Linterpreter_call without return address to return
  adrp  x9, Page(engine_implicit_exception_thrower)
  ldr   x9, [x9, PageOff(engine_implicit_exception_thrower)]
  blr   x9
  brk   #0 // should not return, as exception is thrown inside call

// expects: (ECTYPE_REG: ectype)
// When exception is thrown from CJNative code to interpreter, execution of
// interpreter exception handling starts here
// TODO obtain exception using PostThrowException from RT interface
common_landing_pad:
  bl engine_get_and_clear_pending_exception
  b engine_handle_exception

// expects: (ECTYPE_REG: ectype, x0: exception reference)
// Returns to interpretation loop with exception reference on ACC_REG
// or throws exception reference using CJNative runtime interface if there is no any exception handler in consecutive interpreter frames.
// Precondition: top frame is interpreter frame.
.balign 16
engine_handle_exception:
  mov   EXTRA_REG, x0

  ldr   x0, [x29, #-FUH_SLOT_OFFSET]
  sub   x1, x29, #READER_SLOT_OFFSET // reader
  bl    engine_get_exception_handler

  mov   x12, EXTRA_REG // use x12 (volatile) as temporary exception holder

  adr   x1, .Lexception_handlers
  ldr   w0, [x1, x0, lsl #2]
  add   x1, x1, x0
  br    x1

.Lexception_handlers:
  .long .Lunwind_top_frame - .Lexception_handlers // EXC_HANDLER_NOT_FOUND
  .long .Lcatch_exception  - .Lexception_handlers // EXC_HANDLER_FOUND
  .long .Lhandle_soe       - .Lexception_handlers // EXC_SOE_THROWN

.Lhandle_soe:
  restore_all_iregs x1
  add   sp, sp, STACK_OVERFLOW_REGDUMP_SIZE
  b     .Lhandle_c2i_or_rethrow

.Lcatch_exception:
  // Handler is found, set exception object to ACC_REG and continue interpretation
  str   x12, [ECTYPE_REG, #ECTYPE_IACC_OFFSET]
  b     .Lrestart_interpretation

.Lunwind_top_frame:
  // Restore spilled non-volatile regs before clearing the frame
  ldr   x9, [x29, #-FUH_SLOT_OFFSET]
  ldr   x9, [x9, #FUNCTION_HANDLE_BYTECODE_OFFSET]
  ldr   w1, [x9, #EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  mov   x2, sp
  add   x1, x1, x2            // start of saved regs area

  POP_REGS x8, x1, x9, x2, x3, x4

.Lhandle_c2i_or_rethrow:
  mov   x0, x12 // x0 := exception

  ldr   ECTYPE_REG, [x29, #-ECTYPE_REG_SAVED_SLOT_OFFSET]
  ldp   EXTRA_REG, LITERALS_REG, [x29, #-EXTRA_SAVED_REG_OFFSET]

  // Clear top frame
  mov   sp, x29
  ldp   x29, x30, [sp], #16

  // Check if next method is C2I adapter
  adr   x9, engine_c2i_call_pc_start
  cmp   x30, x9
  b.lo  engine_handle_exception
  adr   x9, engine_c2i_call_pc_end
  cmp   x30, x9
  b.hs  engine_handle_exception

  // Unwind next frame (C2I) and when there are no interpreter frames
  // throw exception using CJNative runtime (as it usually done in AOT code)
.Llast_interpreter_frame_unwinding:

  mov   sp, x29

  // Restore C2I saved regs
  ldp   x19, x20, [sp, #StubCalleeSaveAreaSize]
  ldp   x21, x22, [sp, #StubCalleeSaveAreaSize+0x10]
  ldp   x23, x24, [sp, #StubCalleeSaveAreaSize+0x20]
  ldp   x25, x26, [sp, #StubCalleeSaveAreaSize+0x30]
  ldr   x27,      [sp, #StubCalleeSaveAreaSize+0x40] // do not restore tls reg
  ldp   x29, x30, [sp], #StubFrameContextSize

  // Now stack has the following layout:
  //
  // |                     |
  // |         ...         |
  // | AOT func frame data |
  // |---------------------|
  //
  // x30: AOT func ret ip
  //
  // So br to engine_throw_out_of_interpreter can be seen as blr from AOT function.

  adrp  x9, Page(engine_throw_out_of_interpreter)
  ldr   x9, [x9, PageOff(engine_throw_out_of_interpreter)]
  br    x9
  brk   #0
