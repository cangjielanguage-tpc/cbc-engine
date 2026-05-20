.macro _c2i_emit_iregs_abi count, offset, reg1, reg2, remaining_regs:vararg
    .if \count == 1
      str \reg1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \offset)]
    .endif
    .if \count > 1
      stp \reg1, \reg2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \offset)]

      _c2i_emit_iregs_abi "(\count - 2)", "(\offset + 16)", \remaining_regs
    .endif
.endm

.macro c2i_emit_iregs_abi count
    _c2i_emit_iregs_abi \count, 8, x0, x1, x2, x3, x4, x5, x6, x7, x8
.endm

.macro _c2i_emit_fregs_abi count, offset, reg1, reg2, remaining_regs:vararg
    .if \count == 1
      str \reg1, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \offset)]
    .endif

    .if \count > 1
      stp \reg1, \reg2, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \offset)]

      _c2i_emit_fregs_abi "(\count - 2)", "(\offset + 16)", \remaining_regs
    .endif
.endm

.macro c2i_emit_fregs_abi count
   _c2i_emit_fregs_abi \count, 0, d0, d1, d2, d3, d4, d5, d6, d7
.endm

.macro _i2c_emit_iregs_abi count, offset, reg1, reg2, remaining_regs:vararg
    .if \count == 1
      ldr \reg1, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \offset)]
    .endif
    .if \count > 1
      ldp \reg1, \reg2, [ECTYPE_REG, #(ECTYPE_IREGS_OFFSET + \offset)]

      _i2c_emit_iregs_abi "(\count - 2)", "(\offset + 16)", \remaining_regs
    .endif
.endm

.macro i2c_emit_iregs_abi count
    _i2c_emit_iregs_abi \count, 8, x0, x1, x2, x3, x4, x5, x6, x7, x8
.endm

.macro _i2c_emit_fregs_abi count, offset, reg1, reg2, remaining_regs:vararg
    .if \count == 1

      ldr \reg1, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \offset)]
    .endif
    .if \count > 0
      ldp \reg1, \reg2, [ECTYPE_REG, #(ECTYPE_FREGS_OFFSET + \offset)]

      _i2c_emit_fregs_abi "(\count - 2)", "(\offset + 16)", \remaining_regs
    .endif
.endm

.macro i2c_emit_fregs_abi count
   _i2c_emit_fregs_abi \count, 0, d0, d1, d2, d3, d4, d5, d6, d7
.endm
