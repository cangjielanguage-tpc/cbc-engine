@type default {
  @method default.main()V {
    mov.64 IR3, 0x1
    mov.64 IR1, 0x0
    mov.64 IR2, 0x7
l:  add.64 IR1, IR1, IR2
    sub.64 IR2, IR2, IR3
    branch.if NE, IR2, IRZ, l
    ret.64 IR1
  }
}
