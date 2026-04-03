@type default {
  @methodref default.foo, default foo [ I64, I64 ] I64

  @method default main [ ] I64 {
    mov.64 IR1, 0x7
    mov.64 IR2, 0x0
    call.direct Method(default.foo), IR1
    ret.64 IR1
  }

  @method default foo [ I64, I64] I64 {
    branch.if EQ, IR1, IRZ, r
    add.64 IR2, IR2, IR1
    mov.64 IR3, 0x1
    sub.64 IR1, IR1, IR3
    call.direct Method(default.foo), IR1
r:  mov.64 IR1, IR2
    ret.64 IR1
  }
}
