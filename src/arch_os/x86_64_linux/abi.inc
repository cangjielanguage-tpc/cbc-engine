.intel_syntax noprefix

#include "platform_asm_export.h"

.macro _c2i_emit_iregs_abi count, offset, reg, remaining_regs:vararg
    .if \count > 0
        mov [ECTYPE_REG + ECTYPE_IREGS_OFFSET + \offset], \reg

        _c2i_emit_iregs_abi "(\count - 1)", "(\offset + 8)", \remaining_regs
    .endif
.endm

.macro c2i_emit_iregs_abi count
    _c2i_emit_iregs_abi \count, 8, rdi, rsi, rdx, rcx, r8, r9
.endm

.macro _c2i_emit_fregs_abi count, offset, reg, remaining_regs:vararg
    .if \count > 0
        movsd [ECTYPE_REG + ECTYPE_FREGS_OFFSET + \offset], \reg

        _c2i_emit_fregs_abi "(\count - 1)", "(\offset + 8)", \remaining_regs
    .endif
.endm

.macro c2i_emit_fregs_abi count
   _c2i_emit_fregs_abi \count, 0, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7
.endm

.macro _i2c_emit_iregs_abi count, offset, reg, remaining_regs:vararg
    .if \count > 0
        mov \reg, [ECTYPE_REG + ECTYPE_IREGS_OFFSET + \offset]

        _i2c_emit_iregs_abi "(\count - 1)", "(\offset + 8)", \remaining_regs
    .endif
.endm

.macro i2c_emit_iregs_abi count
    _i2c_emit_iregs_abi \count, 8, rdi, rsi, rdx, rcx, r8, r9
.endm

.macro _i2c_emit_fregs_abi count, offset, reg, remaining_regs:vararg
    .if \count > 0
        movsd \reg, [ECTYPE_REG + ECTYPE_FREGS_OFFSET + \offset]

        _i2c_emit_fregs_abi "(\count - 1)", "(\offset + 8)", \remaining_regs
    .endif
.endm

.macro i2c_emit_fregs_abi count
   _i2c_emit_fregs_abi \count, 0, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7
.endm
