.intel_syntax noprefix

#include "asm_export.h"

.macro declare_hidden sym
  .global \sym
  .hidden \sym
.endm

.extern engine_prepare_bytecode
.extern engine_interpretation_loop
.extern engine_fiber_data_init
.extern engine_get_exception_handler

// exported
declare_hidden engine_c2i_call_pc_start
declare_hidden engine_c2i_call_pc_end
declare_hidden engine_trampolines_dyn_start
declare_hidden engine_trampolines_direct_start
declare_hidden engine_iregs_only_c2i_call
declare_hidden engine_i2i_call
declare_hidden engine_i2c_call
declare_hidden engine_i2_newobject
declare_hidden engine_i2_newarray
declare_hidden engine_i2_gcpoint
declare_hidden engine_universal_direct_function_handles
declare_hidden engine_tls_function
declare_hidden engine_newobject_function
declare_hidden engine_newarray_function
declare_hidden engine_newthread_nret_function
declare_hidden engine_handle_exception
declare_hidden engine_carrier_specific_offset
declare_hidden engine_cjthread_specific_offset
declare_hidden engine_i2_spawn

// --- SECTION BSS ---
.section .bss

.align 16
engine_universal_direct_function_handles:
  .skip 8 * TRAMPOLINE_COUNT

.align 64
engine_tls_function:
  .skip 8
engine_newobject_function:
  .skip 8
engine_newarray_function:
  .skip 8
engine_newthread_nret_function:
  .skip 8
engine_carrier_specific_offset:
  .skip 8
engine_cjthread_specific_offset:
  .skip 8

// --- SECTION TEXT ---
.section .text

#define TLS_REG r15
#define ECTYPE_REG rbx
#define LITERALS_REG r14

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

.macro POP_IREGS mask_reg, stack_reg, exec_info_reg
    movzx   \mask_reg, WORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13
        .set _bit, (\reg - 8)
        test    \mask_reg, (1 << _bit)
        jz      2f
        sub     \stack_reg, 8
        mov     rsi, QWORD PTR [\stack_reg]
        mov     QWORD PTR [ECTYPE_REG + ECTYPE_IREGS_OFFSET + \reg*8], rsi
2:
    .endr
.endm

.macro POP_FREGS mask_reg, stack_reg, exec_info_reg
    movzx   \mask_reg, WORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13,14,15
        .set _bit, (\reg - 8)
        test    \mask_reg, (1 << _bit)
        jz      2f
        sub     \stack_reg, 8
        mov     rsi, QWORD PTR [\stack_reg]
        mov     QWORD PTR [ECTYPE_REG + ECTYPE_FREGS_OFFSET + \reg*8], rsi
2:
    .endr
.endm

.align 16
engine_universal_direct_call:
  lea   r10, [rip + engine_universal_direct_function_handles]
  mov   rax, [r10 + rax * 8] // put FuH on rax
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]


.align 16
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


.align 16
engine_universal_dyn_call:
  mov   r10, [rdi] // get TypeInfo
  // TODO: consider tail array
  mov   r10, [r10 + TYPEINFO_DATA_MT_OFFSET] // data mt array
  mov   rax, [r10 + rax * 0x8] // put FuH on rax
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]


.align 16
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

.align 16
// expects: (TLS_REG: th, rax: fuh)
// System-V abi, except r15 can be changed after context-switches
engine_c2i_call_pc_start:
engine_iregs_only_c2i_call:
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
  jz    .Lc2i_prepare_ectype

.Lc2i_args_preparation:
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8],  rdi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x10], rsi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18], rdx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20], rcx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28], r8
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30], r9

  mov   rdi, rax
  call perform_2i_call

  mov   rax,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]

  pop   rbx // remove FunctionHandle
  pop   rbx
  pop   r12
  pop   r13
  pop   r14
  pop   rbp // do not restore r15

  pop   rbp
  ret

.Lc2i_prepare_ectype:
  push_lst rdi, rsi, rdx, rcx, r8, r9

  mov   rdi, r12
  call  engine_fiber_data_init

  mov   ECTYPE_REG, rax
  mov   rax, [rbp - 0x30] // restore fuh

  pop_lst r9, r8, rcx, rdx, rsi, rdi
  jmp   .Lc2i_args_preparation

engine_c2i_call_pc_end:
  ud2


.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2i_call:
  // jumped from .Linterpreter_call without return address to return
  call  perform_2i_call
  jmp   .Lrestart_interpretation


.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: target)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
engine_i2c_call:
  // jumped from .Linterpreter_call without return address to return
  mov   r10, rdi

  mov   rdi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x10]
  mov   rdx, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18]
  mov   rcx, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20]
  mov   r8,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28]
  mov   r9,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30]

  call  r10

  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8], rax

  jmp   .Lrestart_interpretation

.macro restore_tls // spoils volatile registers!
  call  [rip + engine_tls_function]
  mov   TLS_REG, rax
.endm

// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
.align 16
engine_i2_newobject:
  // jumped from .Linterpreter_call without return address to return
  call  [rip + engine_newobject_function]
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8], rax
  restore_tls
  jmp   .Lrestart_interpretation


// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
// TLS_REG can be changed after context-switch
.align 16
engine_i2_newarray:
  // jumped from .Linterpreter_call without return address to return
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x10]
  call  [rip + engine_newarray_function]
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8], rax
  restore_tls
  jmp   .Lrestart_interpretation

// expects: (ECTYPE_REG: ectype, rdi: safe_point_f)
.align 16
engine_i2_gcpoint:
  call rdi
  restore_tls
  jmp  .Lrestart_interpretation

.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: typeInfo)
// preserved: ECTYPE_REG, rbp
engine_i2_spawn:
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]
  mov   rcx, rdi
  lea   rdi, [rip + engine_fake_execute_closure]
  xor   edx, edx
  call  [rip + engine_newthread_nret_function]
  jmp  .Lrestart_interpretation

.align 16
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: TLS_REG, ECTYPE_REG, rbp
perform_2i_call:
  push  rbp
  mov   rbp, rsp
  push  rdi // rbp - 0x8 : FunctionHandle for frame scanning
  push  ECTYPE_REG // rbp - 0x10
  push  LITERALS_REG // rbp - 0x18

.set OFFS_ADDEND, 0x0

.macro declare_slot var, size
  .set OFFS_ADDEND, OFFS_ADDEND + \size
  .set \var, LOCAL_SLOTS_OFFSET + OFFS_ADDEND
.endm

  declare_slot READER_SLOT, READER_SLOTS_SIZE

  // align stack
  .if (LOCAL_SLOTS_OFFSET + OFFS_ADDEND) % 0x10 != 0
    .set OFFS_ADDEND, OFFS_ADDEND + 0x8
  .endif
  sub   rsp, OFFS_ADDEND
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
  mov   r8, [rax + EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   r8, rsi
  mov   [rbp - READER_SLOT + 0x10], r8 // end
  mov   [rbp - READER_SLOT + 0x8], rsi // start
  mov   [rbp - READER_SLOT], rsi       // cursor

#ifndef NDEBUG
  mov   rdi, [rbp - 0x8]
  mov   rsi, ECTYPE_REG
  call  engine_log_int_start
#endif

  mov   rdx, rsp // save current sp for registers saving to the bottom of frame

  mov   rax, [rbp - 0x8] // load Function Handle
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]

  // current rsp is 16-byte aligned
  mov   r8d, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  sub   rsp, r8

.macro PUSH_IREGS mask_reg, stack_reg, exec_info_reg
    movzx   \mask_reg, WORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_IREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13
        .set _bit, (\reg - 8)
        test    \mask_reg, (1 << _bit)
        jz      .Lsaveireg\reg
        mov     rsi, QWORD PTR [ECTYPE_REG + ECTYPE_IREGS_OFFSET + \reg*ECTYPE_REG_SIZE]
        sub     \stack_reg, ECTYPE_REG_SIZE
        mov     QWORD PTR [\stack_reg], rsi
.Lsaveireg\reg:
    .endr
.endm

.macro PUSH_FREGS mask_reg, stack_reg, exec_info_reg
    movzx   \mask_reg, WORD PTR [\exec_info_reg + EXEC_BYTECODE_INFO_SAVED_FREGS_OFFSET]
    .irp reg, 8,9,10,11,12,13,14,15
        .set _bit, (\reg - 8)
        test    \mask_reg, (1 << _bit)
        jz      .Lsavefreg\reg
        mov     rsi, QWORD PTR [ECTYPE_REG + ECTYPE_FREGS_OFFSET + \reg*ECTYPE_REG_SIZE]
        sub     \stack_reg, ECTYPE_REG_SIZE
        mov     QWORD PTR [\stack_reg], rsi
.Lsavefreg\reg:
    .endr
.endm

  PUSH_IREGS ecx, rdx, rax
  PUSH_FREGS ecx, rdx, rax

.Lrestart_interpretation:
  mov   rdi, ECTYPE_REG
  mov   rsi, rsp
  mov   rdx, TLS_REG
  mov   rcx, LITERALS_REG
  lea   r8, [rbp - READER_SLOT]

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
  mov   rdi, [rbp - 0x8]
  mov   rsi, ECTYPE_REG
  call  engine_log_int_end
#endif

  mov   rax, [rbp - 0x8] // load Function Handle
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]

  mov   edx, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  add   rdx, rsp // add frame size to locate saved registers

  POP_IREGS ecx, rdx, rax
  POP_FREGS ecx, rdx, rax

  mov   LITERALS_REG, [rbp - 0x18]
  mov   ECTYPE_REG, [rbp - 0x10]

  mov   rsp, rbp
  pop   rbp
  ret

// fake execute closure function
.align 16
engine_fake_execute_closure:
  // (rdi: sret, rsi: ()T, rdx: TypeInfo[Future[T]]
  mov   rcx, [rsi + 0x10] // get $i (instantiated) version of closure function
  mov   rdi, rsi
  jmp   rcx

// expects: (ECTYPE_REG: ectype, rdi: exception reference, ectype.SR0: optional[TypeInfo])
// When exception is thrown from CJNative code to interpreter, execution of
// interpreter exception handling starts here
common_landing_pad:
  // TODO obtain pending exception (from CJNative runtime) and put it into rdi
  jmp engine_handle_exception

// expects: (ECTYPE_REG: ectype, rdi: exception reference)
//
// Returns to interpretation loop with exception reference on ectype.SR0
// or throws exception reference using CJNative runtime interface if there is no any exception handler in consecutive interpreter frames.
// Precondition: top frame is interpreter frame.
.align 16
engine_handle_exception:
  mov   r12, rdi

  mov   rdi, QWORD PTR [rbp - 0x8]         // fuh
  lea   rsi, QWORD PTR [rbp - READER_SLOT] // reader
  call  engine_get_exception_handler

  test  al, al
  jz   .Lunwind_top_frame

  // Handler is found, set exception object to SREG and continue interpretation
  mov   QWORD PTR [ECTYPE_REG + ECTYPE_SREGS_OFFSET + 0], r12
  jmp   .Lrestart_interpretation

.Lunwind_top_frame:
  // Restore spilled non-volatile regs before clearing the frame
  mov   rax, QWORD PTR [rbp - 0x8] // fuh
  mov   rax, [rax + FUNCTION_HANDLE_BYTECODE_OFFSET]
  mov   edx, [rax + EXEC_BYTECODE_INFO_FRAME_SIZE_OFFSET]
  add   rdx, rsp // start of saved regs area

  POP_IREGS ecx, rdx, rax
  POP_FREGS ecx, rdx, rax

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

  // Unwind current frame and next (C2I) and throw exception through native frames
  // TODO implement
  xor   eax, eax
  mov   qword ptr [rax], rax

.Lhandle_next_dyn_func:
  mov   rdi, r12
  jmp   engine_handle_exception
