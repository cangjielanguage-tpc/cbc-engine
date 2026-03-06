.intel_syntax noprefix

#include "asm_export.h"
#include "platform_asm_export.h"

.hidden engine_universal_direct_function_handles

.text
.align 16
.local engine_universal_direct_call
.hidden engine_universal_direct_call
engine_universal_direct_call:
  lea   r10, [rip + engine_universal_direct_function_handles]
  mov   rax, [r10 + rax * 8]
  jmp   [rax + FUNCTION_HANDLE_C2CALL_OFFSET]

.text
.align 16
.local engine_trampolines_direct_start
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
.Lend:
  .set SECTION_SIZE, .Lend - engine_trampolines_direct_start
  .if SECTION_SIZE != (DIRECT_CALL_TRAMPOLINE_SIZE * TRAMPOLINE_COUNT)
    .error "wrong trampoline size"
  .endif

.text
.align 16
.local engine_iregs_only_i2c_call
.hidden engine_iregs_only_i2c_call
engine_iregs_only_i2c_call:
  push  rbp
  mov   rbp, rsp

  sub   rsp, CalleeSavedRegistersStubFrameSize
  mov   [rbp - 0x8], r15
  mov   [rbp - 0x10], r14
  mov   [rbp - 0x18], r13
  mov   [rbp - 0x20], r12
  mov   [rbp - 0x20], rbx

#define ECTYPE_REG rbx
#define TLS r15

  mov   ECTYPE_REG, [TLS + TLS_FIBER_DATA_OFFSET]
  mov   ECTYPE_REG, [ECTYPE_REG + FIBER_DATA_ECTYPE_OFFSET]

  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8],  rdi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18], rsi
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20], rdx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28], rcx
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30], r8
  mov   [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x48], r9



  mov   r8,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x8]
  mov   rsi, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x18]
  mov   rdx, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x20]
  mov   rcx, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x28]
  mov   r8,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x30]
  mov   r9,  [ECTYPE_REG + ECTYPE_IREGS_OFFSET + 0x48]

  mov   r14, [rbp - 0x10]
  mov   r13, [rbp - 0x18]
  mov   r12, [rbp - 0x20]
  mov   rbx, [rbp - 0x28]
  mov   rsp, rbp

  pop   rbp
  ret
