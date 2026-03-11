@type default {
  @method default.main()Void {
    mov.64 IR1, 0x7
    call.direct Method(default.foo(I64)I64), IR1
    ret.64 IR1
  }

  @method default.foo(I64)I64 {
    branch.if EQ, IR1, IRZ, r
    mov.64 IR9, IR1
    mov.64 IR3, 0x1
    sub.64 IR1, IR1, IR3
    call.direct Method(default.foo(I64)I64), IR1
    add.64 IR1, IR1, IR9
r:  ret.64 IR1
  }
}
