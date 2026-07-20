.intel_syntax noprefix

#include "asm_export.h"
#include "abi.inc"

.macro declare_hidden sym
  .global \sym
  .hidden \sym
.endm

.extern engine_prepare_bytecode
.extern engine_interpretation_loop
.extern engine_fiber_data_init
.extern engine_get_exception_handler
.extern engine_get_and_clear_pending_exception

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
declare_hidden engine_throw_out_of_interpreter
declare_hidden engine_throw_implicit_exception
declare_hidden engine_implicit_exception_thrower
declare_hidden engine_newobject_function
declare_hidden engine_newarray_function
declare_hidden engine_newthread_nret_function
declare_hidden engine_handle_exception
declare_hidden common_landing_pad
declare_hidden engine_carrier_specific_offset
declare_hidden engine_cjthread_specific_offset
declare_hidden engine_i2_spawn
declare_hidden engine_stack_grow_stub
declare_hidden engine_after_stack_grow

// --- SECTION BSS ---
.section .bss

.balign 16
engine_universal_direct_function_handles:
  .skip 8 * TRAMPOLINE_COUNT

.balign 64
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

// --- SECTION TEXT ---
.section .text

.macro push_lst regs:vararg
  .irp reg, \regs
    push \reg
  .endr
.endm

.macro pop_lst regs:vararg
  .irp reg, \regs
    pop  \reg
  .endr
.endm

.macro PUSH_REGS list_reg, stack_reg, exec_info_reg, tmp
    mov   \list_reg, QWORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    test  \list_reg, \list_reg
    je    .L\@iend
.L\@icopy:
    mov   \tmp, \list_reg
    and   \tmp, 15
    shr   \list_reg, 4
    mov   \tmp, QWORD PTR [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE * \tmp]
    sub   \stack_reg, ECTYPE_REG_SIZE
    mov   QWORD PTR [\stack_reg], \tmp
    test  \list_reg, \list_reg
    jne   .L\@icopy
.L\@iend:
    mov   \list_reg, QWORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    test  \list_reg, \list_reg
    je    .L\@fend
.L\@fcopy:
    mov   \tmp, \list_reg
    and   \tmp, 15
    shr   \list_reg, 4
    mov   \tmp, QWORD PTR [ECTYPE_REG + ECTYPE_FREGS_OFFSET + ECTYPE_REG_SIZE * \tmp]
    sub   \stack_reg, ECTYPE_REG_SIZE
    mov   QWORD PTR [\stack_reg], \tmp
    test  \list_reg, \list_reg
    jne   .L\@fcopy
.L\@fend:
.endm

.macro POP_REGS list_reg, stack_reg, exec_info_reg, tmp1, tmp2
    mov   \list_reg, QWORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    test  \list_reg, \list_reg
    je    .L\@iend
.L\@icopy:
    mov   \tmp2, \list_reg
    and   \tmp2, 15
    shr   \list_reg, 4
    sub   \stack_reg, ECTYPE_REG_SIZE
    mov   \tmp1, QWORD PTR [\stack_reg]
    mov   QWORD PTR [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE * \tmp2], \tmp1
    test  \list_reg, \list_reg
    jne   .L\@icopy
.L\@iend:
    mov   \list_reg, QWORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    test  \list_reg, \list_reg
    je    .L\@fend
.L\@fcopy:
    mov   \tmp2, \list_reg
    and   \tmp2, 15
    shr   \list_reg, 4
    sub   \stack_reg, ECTYPE_REG_SIZE
    mov   \tmp1, QWORD PTR [\stack_reg]
    mov   QWORD PTR [ECTYPE_REG + ECTYPE_FREGS_OFFSET + ECTYPE_REG_SIZE * \tmp2], \tmp1
    test  \list_reg, \list_reg
    jne   .L\@fcopy
.L\@fend:
.endm

.balign 16
engine_universal_direct_call:
  lea   r10, [rip + engine_universal_direct_function_handles]
  mov   rax, [r10 + rax * 8] // put FuH on rax
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]


.balign 16
engine_trampolines_direct_start:
.macro repeat_trampoline_direct count
  .set i, 0
  .rept \count
      mov.d32 eax, i
      jmp.d32 engine_universal_direct_call
      .set i, i + 1
  .endr
.endm
  repeat_trampoline_direct TRAMPOLINE_COUNT


.balign 16
engine_universal_dyn_call:
  mov   r10, [rdi] // get TypeInfo
  // TODO: consider tail array
  mov   r10, [r10 + TYPEINFO_DATA_MT_OFFSET] // data mt array
  mov   rax, [r10 + rax * 0x8] // put FuH on rax
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]


.balign 16
engine_trampolines_dyn_start:
.macro repeat_trampoline_dyn count
  .set i, 0
  .rept \count
      mov.d32 eax, i
      jmp.d32 engine_universal_dyn_call
      .set i, i + 1
  .endr
.endm
  repeat_trampoline_dyn TRAMPOLINE_COUNT
engine_trampolines_dyn_end:

// expects: (TLS_REG: th, rax: fuh)
// System-V abi, except r15 can be changed after context-switches
.macro engine_c2i_call ireg_count freg_count
  push  rbp
  mov   rbp, rsp

  push  r15 // rbp - 0x8 -- unused
  push  r14 // rbp - 0x10
  push  r13 // rbp - 0x18
  push  r12 // rbp - 0x20
  push  rbx // rbp - 0x28

  push  rax // rbp - 0x30 : FunctionHandle that will help to scan saved regs

  mov   ECTYPE_REG, QWORD PTR [rip + engine_carrier_specific_offset]
  mov   r12, [TLS_REG + ECTYPE_REG]
  mov   ECTYPE_REG, QWORD PTR [rip + engine_cjthread_specific_offset]
  lea   r12, [r12 + ECTYPE_REG] // address of slot in fiber data that contains ectype addr
  mov   ECTYPE_REG, [r12]       // ectype addr

  test  ECTYPE_REG, ECTYPE_REG
  jz    .Lc2i_prepare_ectype\@

.Lc2i_args_preparation\@:
  c2i_emit_iregs_abi \ireg_count
  c2i_emit_fregs_abi \freg_count


  mov   rdi, rax
  call perform_2i_call

  mov   rax,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE]
  movq  xmm0, [ECTYPE_REG + ECTYPE_FREGS_OFFSET]

  pop   rbx // remove FunctionHandle
  pop   rbx
  pop   r12
  pop   r13
  pop   r14
  pop   rbp // do not restore r15

  pop   rbp
  ret

.Lc2i_prepare_ectype\@:
  call c2i_prepare_ectype
  jmp   .Lc2i_args_preparation\@
.endm

.balign 16
c2i_prepare_ectype:
  push_lst rdi, rsi, rdx, rcx, r8, r9
  push  rdi // alignment

  mov   rdi, r12
  call  engine_fiber_data_init

  mov   ECTYPE_REG, rax
  mov   rax, [rbp - 0x30] // restore fuh

  pop   rdi
  pop_lst r9, r8, rcx, rdx, rsi, rdi
  ret



.balign 16
// expects: (TLS_REG: th, rax: fuh)
// System-V abi, except r15 can be changed after context-switches
// label that is used to range-check all c2i call variations
engine_c2i_call_pc_start:
engine_all_regs_c2i_call:
  engine_c2i_call IREG_ABI_AMOUNT FREG_ABI_AMOUNT

.balign 16
// variation of c2i call that only passes iregs
engine_iregs_only_c2i_call:
  engine_c2i_call IREG_ABI_AMOUNT 0

// label that is used to range-check all c2i call variations
engine_c2i_call_pc_end:
  ud2

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2i_call:
  // jumped from .Linterpreter_call without return address to return
  call  perform_2i_call
  jmp   .Lrestart_interpretation

// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: target)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
.macro i2c_call iregs_count fregs_count
  mov r10, rdi

  i2c_emit_iregs_abi \iregs_count
  i2c_emit_fregs_abi \fregs_count

  call r10

  mov  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE], rax
  movq [ECTYPE_REG + ECTYPE_FREGS_OFFSET], xmm0

  jmp .Lrestart_interpretation
.endm

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: target)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2c_call:
  // jumped from .Linterpreter_call without return address to return
  i2c_call IREG_ABI_AMOUNT FREG_ABI_AMOUNT

.macro restore_tls // spoils volatile registers!
  call  [rip + engine_tls_function]
  mov   TLS_REG, rax
.endm

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: mask)
// preserved: ECTYPE_REG, rbp
engine_i2_load_generic:
  // mask is expected in form of:
  // [0..4) - dst reg
  // [4..8) - base reg
  // [8..12) - field reg
  // [12..64) - typeInfo

  mov   EXTRA_REG, rdi
  // allocate box
  shr   rdi, 12
  call  [rip + engine_newobject_function]
  mov   rdi, rax // put box as dst param of read generic

  // save box to dst reg
  mov   eax, EXTRA_REGd
  and   eax, 0xf
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8 * rax], rdi

  // obj (base) param
  mov   esi, EXTRA_REGd
  shr   esi, 4
  and   esi, 0xf
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8 * rsi]

  // field param
  mov   edx, EXTRA_REGd
  shr   edx, 8
  and   edx, 0xf
  mov   rdx, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8 * rdx]

  // size param
  mov   rcx, [rdi] // get header
  mov   ecx, [rcx + TYPEINFO_INSTANCESIZE_OFFSET]
  // perform generic read
  call  [rip + engine_read_generic]
  jmp   .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2_newobject:
  // jumped from .Linterpreter_call without return address to return
  call  [rip + engine_newobject_function]
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE], rax
  restore_tls
  jmp   .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2_newobject_acc:
  // jumped from .Linterpreter_call without return address to return
  call  [rip + engine_newobject_function]
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE * ECTYPE_IACC_NUM], rax // move to acc register
  restore_tls
  jmp   .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2_newarray:
  // jumped from .Linterpreter_call without return address to return
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x10]
  call  [rip + engine_newarray_function]
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE], rax
  restore_tls
  jmp   .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, rdi: safe_point_f)
engine_i2_gcpoint:
  call rdi
  restore_tls
  jmp  .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
engine_i2_spawn:
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + ECTYPE_REG_SIZE]
  mov   rcx, rdi
  lea   rdi, [rip + engine_fake_execute_closure]
  xor   edx, edx
  call  [rip + engine_newthread_nret_function]
  jmp  .Lrestart_interpretation

.balign 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: TLS_REG, ECTYPE_REG, rbp
perform_2i_call:
// The final layout of the frame is following:
// +------------------------------------+
// | Return Address                     |
// +------------------------------------+
// | Saved Frame Pointer (Old FP)       |
// +------------------------------------+ <-- FP
// | FuH (rdi)                          |
// +------------------------------------+
// | ECTYPE_REG                         |
// +------------------------------------+
// | LITERALS_REG                       |
// +------------------------------------+
// | EXTRA_REG                          |
// +------------------------------------+>\
// | Reader                             |  \
// +------------------------------------+   -- LOCAL_SLOTS_OFFSET - EXTRA_SAVED_REG_OFFSET
// | Alignment  (0 or 8)                |  /
// +------------------------------------+>/
// | Dynamic Locals (locals_size)       |
// +------------------------------------+ <-- SP
  push  rbp
  mov   rbp, rsp
  push  rdi // rbp - 0x8 : FunctionHandle for frame scanning
  push  ECTYPE_REG // rbp - 0x10
  push  LITERALS_REG // rbp - 0x18
  push  EXTRA_REG // rbp - 0x20

  // reserve slots and align stack
  sub   rsp, LOCAL_SLOTS_OFFSET - EXTRA_SAVED_REG_OFFSET

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
  mov   rax, [rdi + FUNCTION_HANDLE_BYTECODE_OFFSET]
  test  rax, rax
  jnz   .Lhave_bytecode
  // function handle is already on rdi
  call  engine_prepare_bytecode
.Lhave_bytecode:

  mov   LITERALS_REG, [rax + EXEC_BYTECODE_INFO_LITERALS_OFFSET]
  mov   rsi, [rax + EXEC_BYTECODE_INFO_BYTECODE_OFFSET]

  // construct reader
#ifndef NDEBUG
  mov   r8, [rax + EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   r8, rsi
  mov   [rbp - READER_SLOT_OFFSET + 0x10], r8 // end
  mov   [rbp - READER_SLOT_OFFSET + 0x8], rsi // start
#endif
  mov   [rbp - READER_SLOT_OFFSET], rsi // cursor

#ifndef NDEBUG
  mov   rdi, [rbp - FUH_SLOT_OFFSET]
  mov   rsi, ECTYPE_REG
  call  engine_log_int_start
#endif

  mov   rdx, rsp // save current sp for registers saving to the bottom of frame

  mov   rax, [rbp - FUH_SLOT_OFFSET] // load Function Handle
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]

  // current rsp is 16-byte.baligned
  mov   r8d, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  sub   rsp, r8

.Lstack_check:
  mov   r9, [TLS_REG + TLS_STACK_BORDER_OFFSET]
  add   r9, ADDITIONAL_STACK_SPACE
  cmp   rsp, r9
  jbe   expand_stack_or_throw_soe
.Lafter_stack_check:

  PUSH_REGS rcx, rdx, rax, rsi

.Lrestart_interpretation:
  mov   rdi, ECTYPE_REG
  mov   rsi, rsp
  mov   rdx, TLS_REG
  mov   rcx, LITERALS_REG
  lea   r8, [rbp - READER_SLOT_OFFSET]

  call  engine_interpretation_loop

  // Thunk is return on rax: function, rdx: arg.
  // If usual return is needed, interpretation_loop will return null.
  test  rax, rax
  jz    .Lend_interpretation

  mov   rdi, rdx
.Linterpreter_call:
  jmp   rax

.Lend_interpretation:

#ifndef NDEBUG
  mov   rdi, [rbp - FUH_SLOT_OFFSET]
  mov   rsi, ECTYPE_REG
  call  engine_log_int_end
#endif

  mov   rax, [rbp - FUH_SLOT_OFFSET]
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]

  mov   edx, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  add   rdx, rsp // add frame size to locate saved registers

  POP_REGS rcx, rdx, rax, rsi, rdi

  mov   EXTRA_REG, [rbp - EXTRA_SAVED_REG_OFFSET]
  mov   LITERALS_REG, [rbp - LITERALS_REG_SAVED_SLOT_OFFSET]
  mov   ECTYPE_REG, [rbp - ECTYPE_REG_SAVED_SLOT_OFFSET]

  mov   rsp, rbp
  pop   rbp
  ret

// fake execute closure function
.balign 16
engine_fake_execute_closure:
  // (rdi: sret, rsi: ()T, rdx: TypeInfo[Future[T]]
  mov   rcx, [rsi + 0x10] // get $i (instantiated) version of closure function
  mov   rdi, rsi
  jmp   rcx

// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: exception type)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
.balign 16
engine_throw_implicit_exception:
  // jumped from .Linterpreter_call without return address to return
  call  [rip + engine_implicit_exception_thrower]
  ud2 // should not return, as exception is thrown inside call

// expects: (ECTYPE_REG: ectype)
// When exception is thrown from CJNative code to interpreter, execution of
// interpreter exception handling starts here
// TODO obtain exception using PostThrowException from RT interface
.balign 16
common_landing_pad:
  call engine_get_and_clear_pending_exception
  mov rdi, rax
  jmp engine_handle_exception

// expects: (ECTYPE_REG: ectype, rdi: exception reference)
// Returns to interpretation loop with exception reference on ACC_REG
// or throws exception reference using CJNative runtime interface if there is no any exception handler in consecutive interpreter frames.
// Precondition: top frame is interpreter frame.
.balign 16
engine_handle_exception:
  mov   r12, rdi

  mov   rdi, QWORD PTR [rbp - FUH_SLOT_OFFSET]
  lea   rsi, QWORD PTR [rbp - READER_SLOT_OFFSET] // reader
  call  engine_get_exception_handler

  test  al, al
  jz   .Lunwind_top_frame

  // Handler is found, set exception object to ACC_REG and continue interpretation
  mov   QWORD PTR [ECTYPE_REG + ECTYPE_IACC_OFFSET], r12
  jmp   .Lrestart_interpretation

.Lunwind_top_frame:
  // Restore spilled non-volatile regs before clearing the frame
  mov   rax, QWORD PTR [rbp - FUH_SLOT_OFFSET]
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]
  mov   edx, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  add   rdx, rsp // start of saved regs area

  POP_REGS rcx, rdx, rax, rsi, rdi

  mov   LITERALS_REG, [rbp - 0x18]
  mov   ECTYPE_REG, [rbp - 0x10]

  // Clear top frame
  mov rsp, rbp
  pop rbp
  pop rdi // ret addr

  // Check if next method is C2I adapter
  lea   rax, [rip + engine_c2i_call_pc_start]
  cmp   rdi, rax
  jb    .Lhandle_next_dyn_func
  lea   rax, [rip + engine_c2i_call_pc_end]
  cmp   rdi, rax
  jae   .Lhandle_next_dyn_func

  // Unwind C2I frame and when there are no interpreter frames
  // throw exception using CJNative runtime (as it usually done in AOT code)
.Llast_interpreter_frame_unwinding:
  mov   rdi, r12 // set exception pointer as argument

  // TODO use macros here
  // Restore C2I saved regs
  pop   rbx // pop saved fuh
  pop   rbx
  pop   r12
  pop   r13
  pop   r14
  pop   rbp // pop TLD
  pop   rbp

  // Now stack has the following layout:
  //
  // |                     |
  // |         ...         |
  // | AOT func frame data |
  // |---------------------|
  // |   AOT func ret ip   |
  // |---------------------|
  //
  // So jumping to engine_throw_out_of_interpreter can be seen as call from AOT function.

  jmp   [rip + engine_throw_out_of_interpreter]
  ud2

.Lhandle_next_dyn_func:
  mov   rdi, r12
  jmp   engine_handle_exception

.macro save_all_iregs scratch_reg
  .set _dump_size, ((ECTYPE_IREGS_COUNT * ECTYPE_REG_SIZE) + 15) & ~15
  sub   rsp, _dump_size
  .set _offset, 0
  .rept ECTYPE_IREGS_COUNT
    mov    \scratch_reg, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + _offset]
    mov    [rsp + _offset], \scratch_reg
    .set   _offset, _offset + ECTYPE_REG_SIZE
  .endr
.endm

.macro restore_all_iregs scratch_reg
  .set _dump_size, ((ECTYPE_IREGS_COUNT * ECTYPE_REG_SIZE) + 15) & ~15
  .set _offset, 0
  .rept ECTYPE_IREGS_COUNT
    mov    \scratch_reg, [rsp + _offset]
    mov    [ECTYPE_REG + ECTYPE_IREGS_OFFSET + _offset], \scratch_reg
    .set   _offset, _offset + ECTYPE_REG_SIZE
  .endr
  add   rsp, _dump_size
.endm

// expects: (TLS_REG: valid TLS, rsp: top of stack (including the whole current frame), r8: frame size)
// preserves: all registers
// Expands stack and returns to perform 2i call or throws SOE if stack expansion is impossible
.balign 16
expand_stack_or_throw_soe:
  add   rsp, r8    // rsp state on prologue
  save_all_iregs rax

  mov   rax, 0x20  // adjusted size (TODO: its meaning is not really known, need to investigate more)
  call  [rip + engine_stack_grow_stub]
engine_after_stack_grow:

  restore_all_iregs rax
  mov   rdx, rsp   // update rdx after stack movement
  sub   rsp, r8    // stack grow stub preserves all registers, so r8 will not be changed

  // restore spoiled rax value
  mov   rax, [rbp - FUH_SLOT_OFFSET]
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]

  jmp   .Lafter_stack_check
