.intel_syntax noprefix

#include "asm_export.h"

.hidden engine_universal_direct_function_handles
.hidden engine_prepare_bytecode
.hidden engine_interpretation_loop


.text
.align 16
.global engine_universal_direct_call
.hidden engine_universal_direct_call
engine_universal_direct_call:
  lea   r10, [rip + engine_universal_direct_function_handles]
  mov   rax, [r10 + rax * 8]
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


.text
.align 16
.global engine_iregs_only_c2i_call
.hidden engine_iregs_only_c2i_call
engine_iregs_only_c2i_call:
  push  rbp
  mov   rbp, rsp

  push  r15 // rbp - 0x8
  push  r14 // rbp - 0x10
  push  r13 // rbp - 0x18
  push  r12 // rbp - 0x20
  push  rbx // rbp - 0x28

#define TLS r15
#define ECTYPE_REG rbx

  mov   rbx, [TLS + TLS_FIBER_DATA_OFFSET]
  mov   ECTYPE_REG, [ECTYPE_REG + FIBER_DATA_ECTYPE_OFFSET]
  push  ECTYPE_REG // rbp - 0x30

  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8],  rdi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18], rsi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20], rdx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28], rcx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30], r8
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x48], r9

  mov   rdi, ECTYPE_REG
  mov   rcx, rax
  mov   rdx, r15
  call perform_2i_call

  pop   ECTYPE_REG
  mov   rax,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]

  pop   rbx
  pop   rbx
  pop   r12
  pop   r13
  pop   r14
  pop   rbp // r15

  pop   rbp
  ret


.text
.align 16
.global engine_i2i_call
.hidden engine_i2i_call
engine_i2i_call: // (rdi: ectype, rsi: oldFrame, rdx: th, rcx:fuh)
  mov   rsp, rsi
  lea   rax, [rip + restart_interpretation]
  push  rax
  // jmp   perform_2i_call
  // fall through
perform_2i_call:  // (rdi: ectype, rdx: th, rcx: fuh)
  push  rbp
  mov   rbp, rsp
  // store function handle right after saved registers
  push  rcx // rbp - 0x8 (fuh)
  push  rdi // rbp - 0x10 (ectype)
  .set FUH_SLOT,    0x8
  .set ECTYPE_SLOT, 0x10

  .set SLOT_OFFS,   ECTYPE_SLOT
  .set OFFS_ADDEND, 0x0

.macro declare_slot var, n
  .set OFFS_ADDEND, OFFS_ADDEND + \n
  .set \var, SLOT_OFFS + OFFS_ADDEND
.endm

  declare_slot LITERALS_SLOT, 0x8
  declare_slot ECTYPE_SLOT, 0x8
#ifndef NDEBUG
  declare_slot READER_SLOT, 0x18
#else
  declare_slot READER_SLOT, 0x8
#endif
  .if OFFS_ADDEND % 0x10 != 0
    .set OFFS_ADDEND, OFFS_ADDEND + 0x8
  .endif
  sub   rsp, OFFS_ADDEND
  // FIXME: perform stack check here!

  // DO NOT RELY ON CALLEE-SAVED REGISTERS.
  // We need to support fibers with movable stacks,
  // but to do it safely we need to adjust pointers to stack.
  // It is impossible to do correcly in C/C++ frames.
  // So, we will rely on "tail calls" from C++ interpreter loop
  // to managed compiled code.
  //
  // The problem is that we can't rely on tail call optimization
  // and can't force such behaviour reliably (early 2026).
  // Instead, we will call wrapper-function that would forcibly
  // restore stack layout to the state before interpretation loop was called.
  // The wrapper *would not* restore callee-saved registers.
  mov   r15, rdx
  mov   rax, [rcx + FUNCTION_HANDLE_BYTECODE_OFFSET]
  test  rax, rax
  jnz   .Lhave_bytecode
  mov   rdi, rax
  call  engine_prepare_bytecode
.Lhave_bytecode:

  mov   rcx, [rax + EXEC_BYTECODE_INFO_LITERALS_OFFSET]
  mov   rsi, [rax + EXEC_BYTECODE_INFO_BYTECODE_OFFSET]
  mov   [rbp - READER_SLOT], rsi
  mov   [rbp - LITERALS_SLOT], rcx
#ifndef NDEBUG
  mov   [rbp - READER_SLOT + 0x8], rsi
  mov   r8, [rax + EXEC_BYTECODE_INFO_BYTECODE_SIZE_OFFSET]
  add   r8, rsi
  mov   [rbp - READER_SLOT + 0x10], r8
#endif

  // FIXME: init frame here (prepare frame metadata to be 16-byte aligned!)

restart_interpretation:
  mov   rdi, [rbp - ECTYPE_SLOT]
  mov   rsi, rsp
  mov   rdx, r15
  lea   r8, [rbp - READER_SLOT]

  // the pushed return address could be:
  // - stay the same (in case of normal return);
  // - patched to `restart_interpretation`;
  // - patched to exception throw;
  call  engine_interpretation_loop
  mov   rsp, rbp
  pop   rbp
  ret
