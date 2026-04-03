@type default {
  @method default.main()V {
    addi.64 IR1, IR1, 0x30000
    addi.64 IR1, IR1, -0x30001
    muli.64 IR1, IR1, 0x30000
    divi.64 IR1, IR1, 0x30000
    muli.64 IR1, IR1, -0x40000
    divi.64 IR1, IR1, -0x40000
    umuli.64 IR1, IR1, 0x4000000
    udivi.64 IR1, IR1, 0x4000000
    ret.64 IR1
  }
}
