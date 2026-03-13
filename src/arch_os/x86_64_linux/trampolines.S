.intel_syntax noprefix

#include "asm_export.h"

.global engine_universal_direct_function_handles
.global engine_prepare_bytecode
.global engine_interpretation_loop

// Fake Ectype* instance represented as global
// FIXME: get Ectype* instance from ThreadHandle
.bss
.align 16
.local engine_fake_ectype
engine_fake_ectype:
  .skip 1024

.text
.align 16
.global engine_universal_direct_call
.hidden engine_universal_direct_call
engine_universal_direct_call:
  lea   r10, [rip + engine_universal_direct_function_handles]
  mov   rax, [r10 + rax * 8] // put FuH on rax
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]


.text
.align 16
.global engine_trampolines_direct_start
.hidden engine_trampolines_direct_start
engine_trampolines_direct_start:
.macro repeat_call count
  .set i, 0
  .rept \count
      mov.d32 eax, i
      jmp.d32 engine_universal_direct_call
      .set i, i + 1
  .endr
.endm
  repeat_call TRAMPOLINE_COUNT

#define TLS_REG r15
#define ECTYPE_REG rbx

.text
.align 16
.global engine_iregs_only_c2i_call
.hidden engine_iregs_only_c2i_call
// System-V abi, except r15 can be changed after context-switches
engine_iregs_only_c2i_call:
  push  rbp
  mov   rbp, rsp

  push  r14 // rbp - 0x8
  push  r13 // rbp - 0x10
  push  r12 // rbp - 0x18
  push  rbx // rbp - 0x20
  push  rax // rbp - 0x28 : FunctionHandle that will help to scan saved regs
  push  rax // rbp - 0x30 alignment


// FIXME: get Ectype* from ThreadHandle
//  mov   ECTYPE_REG, [TLS + TLS_FIBER_DATA_OFFSET]
//  mov   ECTYPE_REG, [ECTYPE_REG + FIBER_DATA_ECTYPE_OFFSET]
  lea   ECTYPE_REG, [rip + engine_fake_ectype]

  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8],  rdi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x10], rsi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18], rdx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20], rcx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28], r8
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30], r9

  mov   rdi, rax
  call perform_2i_call

  mov   rax,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]

  pop   rbx
  pop   rbx // remove FunctionHandle
  pop   rbx
  pop   r12
  pop   r13
  pop   r14

  pop   rbp
  ret


.text
.align 16
.global engine_i2i_call
.hidden engine_i2i_call
// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: TLS_REG, ECTYPE_REG, rbp
engine_i2i_call:
  jmp perform_2i_call

// expects: (ECTYPE_REG: ectype, TLS_REG: th, rdi: fuh)
// preserved: TLS_REG, ECTYPE_REG, rbp
perform_2i_call:
  push  rbp
  mov   rbp, rsp
  push  rdi // rbp - 0x8 : FunctionHandle for frame scanning
  push  rdi // rbp - 0x10 : location for frame base

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

#define LITERALS_REG r14
  mov   LITERALS_REG, [rax + EXEC_BYTECODE_INFO_LITERALS_OFFSET]
  mov   rsi, [rax + EXEC_BYTECODE_INFO_BYTECODE_OFFSET]
#ifndef NDEBUG
  mov   r8, [rax + EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   r8, rsi
  mov   [rbp - READER_SLOT + 0x10], r8
  mov   [rbp - READER_SLOT + 0x8], rsi
#endif
  mov   [rbp - READER_SLOT], rsi

  // current rsp is 16-byte aligned
  // FIXME: init frame here (prepare frame metadata to be 16-byte aligned!)

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
  call  rax
  jmp   .Lrestart_interpretation

.Lend_interpretation:
  mov   rsp, rbp
  pop   rbp
  ret
