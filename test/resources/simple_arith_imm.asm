@type default {
  @method default.main()V {
    mov.64 IR3, 0x1
    mov.64 IR1, 0x0
    mov.64 IR2, 0x7
    addi.64 IR2, IR2, 0x14
    xori.64 IR1, IR3, 0x5
    div.64 IR3, IR2, IR1
    subi.64 IR1, IR3, 0x4
    ret.64 IR1
  }
}
