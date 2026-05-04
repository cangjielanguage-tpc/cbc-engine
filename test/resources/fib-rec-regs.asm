@type default {
  @methodref default.fib, DIRECT default fib [ I64 ] I64

  @method default main [ ] I64 {
    mov.64 IR1, 0x7
    call.direct Method(default.fib), IR1
    ret.64 IR1
  }

  @method default fib [ I64 ] I64 {
    @methodcode usedNonVolIRegsMask 0x3

    live.prim [ IR1 ]
    branch.if LE, IR1, 0x1, r
    mov.64 IR8, IR1
    subi.64 IR1, IR1, 0x1
    call.direct Method(default.fib), IR1
    mov.64 IR9, IR1
    dead [ IR1 ]
    subi.64 IR1, IR8, 0x2
    dead [ IR8 ]
    call.direct Method(default.fib), IR1
    add.64 IR1, IR1, IR9
    dead [ IR9 ]
r:  ret.64 IR1
  }
}
