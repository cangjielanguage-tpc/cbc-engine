@type default {
  @methodref default.fib, DIRECT default fib [ I64 ] I64

  @method default main [ ] I64 {
    mov.64 IR1, 0x7
    call.direct Method(default.fib), IR1
    ret.64 IR1
  }

  @method default fib [ I64 ] I64 {
    @methodcode untypedStackSlotsCount 0x1

    live.prim [ IR1 ]
    branch.if LE, IR1, 0x1, r
    st.uslot IR1, CbcKind(I64), 0
    subi.64 IR1, IR1, 0x1
    call.direct Method(default.fib), IR1
    ld.uslot IR2, CbcKind(I64), 0
    dead [ 0 ]
    st.uslot IR1, CbcKind(I64), 0
    dead [ IR1 ]
    subi.64 IR1, IR2, 0x2
    dead [ IR2 ]
    call.direct Method(default.fib), IR1
    ld.uslot IR2, CbcKind(I64), 0
    add.64 IR1, IR1, IR2
    dead [ IR2, 0 ]
r:  ret.64 IR1
  }
}
